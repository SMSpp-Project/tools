/*--------------------------------------------------------------------------*/
/*------------------------- File timing_harness.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Micro-benchmark for the Lagrangian-iteration timing study driven by
 * run-lagrangian-timing: given a directory of single ThermalUnitBlock netCDF
 * dumps named TUB-<unit>-<iteration>.nc4 (produced by tools/ucblock_solver
 * compiled with -DSAVE_TUB=1 while solving a UCBlock with the
 * LagrangianDualSolver), it times FOUR solvers and prints a CSV
 * "unit,iter,solver,time_us,phase".
 *
 *   - "stdDP" : ThermalUnitDPSolver     (standard DP, exact integer, serial)
 *   - "parDP" : ThermalUnitDPSolver     (same, intMaxThread 0 = all cores as
 *               FastFlow workers on compute_EDPs; the solver itself falls back
 *               to serial below the TUDPS_PAR_MIN_N horizon threshold)
 *   - "extDP" : ThermalUnitExtDPSolver  (extended DP, exact integer)
 *   - "MILP"  : a :MILPSolver on the T + Perspective-Cuts formulation,
 *               solved as a MILP (intRelaxIntVars 0)
 *
 * The timing mirrors how a LagrangianDualSolver actually uses the single-unit
 * solver: the instance is loaded ONCE and the Solver is attached ONCE; from one
 * Lagrangian iteration to the next ONLY the Lagrangian costs change (the
 * active-power linear term and, when the unit prices reserve, the primary/
 * secondary spinning-reserve costs), and the dual pushes them into the same
 * attached Block via set_linear_term() / set_*_spinning_reserve_cost() and
 * re-solves: it never detaches/re-attaches the Solver or rebuilds the model.
 * Accordingly, for each unit this harness deserializes its first dump, attaches
 * the Solver, and times:
 *   - the first solve once  -> phase "cold" : the one-off model build + solve;
 *   - every later dump       -> phase "warm" : push that dump's costs (active and
 *     reserve), then compute(), i.e. the warm re-optimization the dual pays per
 *     iteration (Gurobi keeps its presolved model and warm-starts; stdDP keeps
 *     its graph; extDP rebuilds its value functions). dp_reps/milp_reps median
 *     each warm re-solve by toggling the previous costs back in between solves.
 * The dumps are strided (intEverykIt apart), so a warm re-solve here spans many
 * dual steps at once: it is a conservative upper bound on the true per-step cost.
 *
 * Nothing here is tied to a specific solver: each solver is attached by reading
 * a BlockSolverConfig from a .txt file (the SMS++ standard way -- never
 * new_Solver()/set_par() by hand), and the MILP formulation comes from a
 * BlockConfig .txt file. The config files live in <dir>/config/ (the driver
 * copies the tool's shared config/ there, overlaid with this study's own
 * config/). The files are:
 *
 *   - TUBSCfg-stdDP.txt : BlockSolverConfig -> ThermalUnitDPSolver (serial)
 *   - TUBSCfg-parDP.txt : BlockSolverConfig -> ThermalUnitDPSolver (parallel)
 *   - TUBSCfg-DP.txt    : BlockSolverConfig -> ThermalUnitExtDPSolver
 *   - TUBSCfg-MILP.txt  : BlockSolverConfig -> a :MILPSolver (integer)
 *   - TUBCfg-tpc.txt    : BlockConfig       -> the T+P/C formulation (wf = 9)
 *
 * All four solve the SAME mathematical (integer) single-unit problem: the two
 * DPs are exact integer solvers, so the MILP is run on the integer problem too
 * (not the LP relaxation) for an apples-to-apples comparison.
 *
 * It is built as its own small executable target next to ucblock_solver and is
 * driven by run-lagrangian-timing.
 *
 * usage: timing_harness <dir> [dp_reps=30] [stride=1] [milp_reps=3]
 *        stride > 1 samples only the iterations with iter % stride == 0.
 *        milp_reps == 0 skips the (slow) MILP solves entirely.
 */
/*--------------------------------------------------------------------------*/

#include <chrono>
#include <map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <regex>
#include <cstdlib>

#include "Block.h"
#include "BlockSolverConfig.h"
#include "ThermalUnitBlock.h"

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/

static double median( std::vector< double > & v )
{
 std::sort( v.begin() , v.end() );
 return( v[ v.size() / 2 ] );
 }

/*--------------------------------------------------------------------------*/
/* Deserialize a BlockSolverConfig from a .txt file (the SMS++ standard way of
 * picking and configuring a Solver). Returns nullptr if the file is missing or
 * is not a BlockSolverConfig. */

static BlockSolverConfig * get_bsc( const std::string & path )
{
 if( ! std::filesystem::exists( path ) )
  return( nullptr );
 return( dynamic_cast< BlockSolverConfig * >(
                                       Configuration::deserialize( path ) ) );
 }

/*--------------------------------------------------------------------------*/
/* The Lagrangian costs a LagrangianDualSolver changes from one iteration to the
 * next and pushes into the inner unit: the per-period active-power linear term
 * (set_linear_term, the dual of the power balance) and the primary/secondary
 * spinning-reserve linear costs (set_*_spinning_reserve_cost, the dual of the
 * reserve demand). The reserve vectors are empty when the unit prices no
 * reserve (energy-only instances or units that offer none). */

struct Costs { std::vector< double > lin, pr, sc; };

/* Read all the changing costs from a TUB dump (empty .lin on failure). */
static Costs read_costs( const std::string & nc4 )
{
 Costs c;
 Block * b = Block::deserialize( nc4 );
 if( auto tub = dynamic_cast< ThermalUnitBlock * >( b ) ) {
  auto n = tub->get_time_horizon();
  c.lin.resize( n );
  for( decltype( n ) t = 0 ; t < n ; ++t )
   c.lin[ t ] = tub->get_linear_term( t );  // handles the broadcast case
  c.pr = tub->get_primary_spinning_reserve_cost();    // copy (may be empty)
  c.sc = tub->get_secondary_spinning_reserve_cost();
  }
 delete b;
 return( c );
 }

/* Push the costs into the attached unit, exactly the modifications the dual
 * issues each iteration; the attached Solver consumes them and re-optimizes on
 * the next compute(). Reserve setters are skipped when the unit prices none. */
static void apply_costs( ThermalUnitBlock * tub , const Costs & c )
{
 tub->set_linear_term( c.lin.begin() );
 if( ! c.pr.empty() )
  tub->set_primary_spinning_reserve_cost( c.pr.begin() );
 if( ! c.sc.empty() )
  tub->set_secondary_spinning_reserve_cost( c.sc.begin() );
 }

/*--------------------------------------------------------------------------*/

struct Row { int iter; double time_us; const char * phase; };

/*--------------------------------------------------------------------------*/
/* Time a Solver over a unit's iteration sequence the way a LagrangianDualSolver
 * uses it (see the file header). dumps is (iter, path) sorted by iter; if
 * bc_path is non-empty it is the formulation BlockConfig (T+P/C) applied -- and
 * the abstract representation generated -- before the Solver is attached, as a
 * :MILPSolver needs (the DP solvers read the physical data and ignore it).
 * Returns one Row per dump (the first "cold", the rest "warm"); empty on any
 * setup failure. */

static std::vector< Row > time_unit(
                  const std::vector< std::pair< int , std::string > > & dumps ,
                  const std::string & bsc_path , const std::string & bc_path ,
                  int reps )
{
 std::vector< Row > out;
 if( dumps.empty() )
  return( out );

 // load the FIRST dump as the persistent Block - - - - - - - - - - - - - - -
 Block * b = Block::deserialize( dumps.front().second );
 auto tub = dynamic_cast< ThermalUnitBlock * >( b );
 if( ! tub ) { delete b; return( out ); }

 if( ! bc_path.empty() ) {
  // formulation from a BlockConfig file (set_BlockConfig takes ownership), then
  // generate the abstract representation a :MILPSolver needs; set_reserve_vars
  // generates the reserve vars the parent UCBlock would (no-op without reserve
  // data), as tests/ThermalUnitBlock_Solver does
  auto bc = dynamic_cast< BlockConfig * >(
                                    Configuration::deserialize( bc_path ) );
  if( ! bc ) { delete b; return( out ); }
  tub->set_BlockConfig( bc );
  tub->set_reserve_vars( 3 );
  tub->generate_abstract_variables();
  tub->generate_objective( nullptr );
  }

 BlockSolverConfig * bsc = get_bsc( bsc_path );
 if( ! bsc ) { delete b; return( out ); }
 bsc->apply( tub );                       // attach + configure the Solver once
 if( tub->get_registered_solvers().empty() ) {
  bsc->clear(); bsc->apply( tub ); delete bsc; delete b; return( out ); }
 Solver * s = tub->get_registered_solvers().back();

 auto timed_compute = [ & ]() -> double {
  auto t0 = std::chrono::steady_clock::now();
  s->compute();
  auto t1 = std::chrono::steady_clock::now();
  return( std::chrono::duration< double , std::micro >( t1 - t0 ).count() );
  };

 // cold: the one-off first solve (model build + solve) - - - - - - - - - - -
 out.push_back( { dumps.front().first , timed_compute() , "cold" } );

 // warm: push each later iteration's Lagrangian costs into the SAME Block and
 // re-optimize, as the dual does (no detach/re-attach, no rebuild) - - - - - -
 Costs prev = read_costs( dumps.front().second );
 for( std::size_t i = 1 ; i < dumps.size() ; ++i ) {
  Costs cur = read_costs( dumps[ i ].second );
  if( cur.lin.empty() ) continue;
  std::vector< double > ts; ts.reserve( reps );
  for( int r = 0 ; r < reps ; ++r ) {
   apply_costs( tub , cur );              // issues the Modifications the Solver
   ts.push_back( timed_compute() );       // consumes -> warm re-optimization
   // re-dirty with the previous costs so the next timed solve does real work
   // (re-setting the same costs would be a no-op the Solver could skip)
   if( ( r + 1 < reps ) && ( ! prev.lin.empty() ) ) {
    apply_costs( tub , prev );
    s->compute();
    }
   }
  out.push_back( { dumps[ i ].first , median( ts ) , "warm" } );
  prev = std::move( cur );
  }

 s->set_Block( nullptr );
 bsc->clear(); bsc->apply( tub );         // detach the Solver (standard cleanup)
 delete bsc;
 delete b;
 return( out );
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 if( argc < 2 ) {
  std::cerr << "usage: timing_harness <dir> [dp_reps] [stride] [milp_reps]\n";
  return( 1 );
  }
 std::string dir = argv[ 1 ];
 int dp_reps   = argc > 2 ? std::atoi( argv[ 2 ] ) : 30;
 int stride    = argc > 3 ? std::atoi( argv[ 3 ] ) : 1;
 int milp_reps = argc > 4 ? std::atoi( argv[ 4 ] ) : 3;

 // config directory: <dir>/config (the driver merges the tool's shared
 // config/ and this study's config/ there)
 const std::string cfg = dir + "/config";
 const std::string stdDP_bsc = cfg + "/TUBSCfg-stdDP.txt";
 const std::string parDP_bsc = cfg + "/TUBSCfg-parDP.txt";
 const std::string extDP_bsc = cfg + "/TUBSCfg-DP.txt";
 const std::string milp_bsc  = cfg + "/TUBSCfg-MILP.txt";
 const std::string tpc_bc    = cfg + "/TUBCfg-tpc.txt";

 // collect the dumps, grouped by unit and sorted by iteration: a unit's whole
 // iteration sequence is timed together (load once, warm re-solve along it)
 std::map< int , std::vector< std::pair< int , std::string > > > by_unit;
 std::regex re( "TUB-([0-9]+)-([0-9]+)\\.nc4" );
 for( auto & e : std::filesystem::directory_iterator( dir ) ) {
  std::smatch mm; std::string ff = e.path().filename().string();
  if( ! std::regex_match( ff , mm , re ) ) continue;
  int unit = std::stoi( mm[ 1 ] ) , iter = std::stoi( mm[ 2 ] );
  if( stride > 1 && ( iter % stride != 0 ) ) continue;
  by_unit[ unit ].emplace_back( iter , e.path().string() );
  }
 for( auto & [ unit , dumps ] : by_unit )
  std::sort( dumps.begin() , dumps.end() );

 bool milp_warned = false;

 std::cout << "unit,iter,solver,time_us,phase\n";
 auto emit = [ & ]( int unit , const char * name ,
                    const std::vector< Row > & rows ) {
  for( auto & r : rows )
   std::cout << unit << "," << r.iter << "," << name << "," << r.time_us
             << "," << r.phase << "\n";
  };

 for( auto & [ unit , dumps ] : by_unit ) {
  emit( unit , "stdDP" , time_unit( dumps , stdDP_bsc , "" , dp_reps ) );
  emit( unit , "parDP" , time_unit( dumps , parDP_bsc , "" , dp_reps ) );
  emit( unit , "extDP" , time_unit( dumps , extDP_bsc , "" , dp_reps ) );

  if( milp_reps > 0 ) {
   auto m = time_unit( dumps , milp_bsc , tpc_bc , milp_reps );
   if( m.empty() && ! milp_warned ) {
    std::cerr << "timing_harness: MILP timing skipped (missing config "
                 "or no :MILPSolver available)\n";
    milp_warned = true;
    }
   emit( unit , "MILP" , m );
   }
  }
 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*---------------------- End File timing_harness.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
