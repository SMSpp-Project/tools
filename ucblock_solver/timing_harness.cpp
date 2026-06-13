/*--------------------------------------------------------------------------*/
/*------------------------- File timing_harness.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Micro-benchmark for the Lagrangian-iteration timing study driven by
 * run-lagrangian-timing: given a directory of single ThermalUnitBlock netCDF
 * dumps named TUB-<unit>-<iteration>.nc4 (produced by tools/ucblock_solver
 * compiled with -DSAVE_TUB=1 while solving a UCBlock with the
 * LagrangianDualSolver), it times FOUR solvers on each dump and prints a CSV
 * "unit,iter,solver,time_us" (the median solve time over a few reps):
 *
 *   - "stdDP" : ThermalUnitDPSolver     (standard DP, exact integer, serial)
 *   - "parDP" : ThermalUnitDPSolver     (same, intMaxThread 0 = all cores as
 *               FastFlow workers on compute_EDPs; the solver itself falls back
 *               to serial below the TUDPS_PAR_MIN_N horizon threshold)
 *   - "extDP" : ThermalUnitExtDPSolver  (extended DP, exact integer)
 *   - "MILP"  : a :MILPSolver on the T + Perspective-Cuts formulation,
 *               solved as a MILP (intRelaxIntVars 0)
 *
 * Nothing here is tied to a specific solver: each solver is attached by reading
 * a BlockSolverConfig from a .txt file (the SMS++ standard way -- never
 * new_Solver()/set_par() by hand), and the MILP formulation comes from a
 * BlockConfig .txt file. The config files live in <dir>/config/ (the driver
 * symlinks the tool's config/ there). The files are:
 *
 *   - TUBSCfg-stdDP.txt : BlockSolverConfig -> ThermalUnitDPSolver (serial)
 *   - TUBSCfg-parDP.txt : BlockSolverConfig -> ThermalUnitDPSolver (parallel)
 *   - TUBSCfg-DP.txt    : BlockSolverConfig -> ThermalUnitExtDPSolver
 *   - TUBSCfg-MILP.txt  : BlockSolverConfig -> a :MILPSolver (integer)
 *   - TUBCfg-tpc.txt    : BlockConfig       -> the T+P/C formulation (wf = 9)
 *
 * The three solve the SAME mathematical (integer) single-unit problem: the two
 * DPs are exact integer solvers, so the MILP is run on the integer problem too
 * (not the LP relaxation) for an apples-to-apples comparison.
 *
 * It is built as its own small executable target next to ucblock_solver (see
 * CMakeLists.txt) and is driven by run-lagrangian-timing.
 *
 * usage: timing_harness <dir> [dp_reps=30] [stride=1] [milp_reps=3]
 *        stride > 1 samples only the iterations with iter % stride == 0.
 *        milp_reps == 0 skips the (slow) MILP solves entirely.
 */
/*--------------------------------------------------------------------------*/

#include <chrono>
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
/* Time a Solver attached to a freshly deserialized ThermalUnitBlock through the
 * BlockSolverConfig in bsc_path. If bc_path is non-empty it is a BlockConfig
 * (the formulation, e.g. T+P/C) applied -- and the abstract representation
 * generated -- before attaching the solver; this is needed by a :MILPSolver,
 * while the DP solvers read the physical unit data directly and ignore it.
 * Detaching/re-attaching the Block between reps forces a full re-solve.
 * Returns -1 on any setup failure (missing config, no solver, wrong Block). */

static double time_solver( const std::string & nc4 , const std::string & bsc_path ,
                           const std::string & bc_path , int reps )
{
 Block * b = Block::deserialize( nc4 );
 auto tub = dynamic_cast< ThermalUnitBlock * >( b );
 if( ! tub ) { delete b; return( -1.0 ); }

 if( ! bc_path.empty() ) {
  // formulation from a BlockConfig file (set_BlockConfig takes ownership), then
  // generate the abstract representation a :MILPSolver needs; set_reserve_vars
  // generates the reserve vars the parent UCBlock would (no-op without reserve
  // data), as tests/ThermalUnitBlock_Solver does
  auto bc = dynamic_cast< BlockConfig * >(
                                    Configuration::deserialize( bc_path ) );
  if( ! bc ) { delete b; return( -1.0 ); }
  tub->set_BlockConfig( bc );
  tub->set_reserve_vars( 3 );
  tub->generate_abstract_variables();
  tub->generate_objective( nullptr );
  }

 BlockSolverConfig * bsc = get_bsc( bsc_path );
 if( ! bsc ) { delete b; return( -1.0 ); }
 bsc->apply( tub );                       // attach + configure the Solver
 if( tub->get_registered_solvers().empty() ) {
  bsc->clear(); bsc->apply( tub ); delete bsc; delete b; return( -1.0 ); }
 Solver * s = tub->get_registered_solvers().back();

 std::vector< double > ts; ts.reserve( reps );
 for( int r = 0 ; r < reps ; ++r ) {
  s->set_Block( nullptr );
  s->set_Block( tub );
  auto t0 = std::chrono::steady_clock::now();
  s->compute();
  auto t1 = std::chrono::steady_clock::now();
  ts.push_back( std::chrono::duration< double , std::micro >( t1 - t0 ).count() );
  }
 s->set_Block( nullptr );

 bsc->clear(); bsc->apply( tub );         // detach the Solver (standard cleanup)
 delete bsc;
 delete b;
 return( median( ts ) );
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

 // config directory: <dir>/config (the driver symlinks the tool's config/
 // there)
 const std::string cfg = dir + "/config";
 const std::string stdDP_bsc = cfg + "/TUBSCfg-stdDP.txt";
 const std::string parDP_bsc = cfg + "/TUBSCfg-parDP.txt";
 const std::string extDP_bsc = cfg + "/TUBSCfg-DP.txt";
 const std::string milp_bsc  = cfg + "/TUBSCfg-MILP.txt";
 const std::string tpc_bc    = cfg + "/TUBCfg-tpc.txt";

 std::vector< std::filesystem::path > files;
 std::regex re( "TUB-([0-9]+)-([0-9]+)\\.nc4" );
 for( auto & e : std::filesystem::directory_iterator( dir ) ) {
  std::smatch mm; std::string ff = e.path().filename().string();
  if( ! std::regex_match( ff , mm , re ) ) continue;
  if( stride > 1 && ( std::stoi( mm[ 2 ] ) % stride != 0 ) ) continue;
  files.push_back( e.path() );
  }
 std::sort( files.begin() , files.end() );

 bool milp_warned = false;

 std::cout << "unit,iter,solver,time_us\n";
 for( auto & p : files ) {
  std::smatch m; std::string fn = p.filename().string();
  std::regex_match( fn , m , re );
  int unit = std::stoi( m[ 1 ] ) , iter = std::stoi( m[ 2 ] );

  double t_std = time_solver( p.string() , stdDP_bsc , "" , dp_reps );
  double t_par = time_solver( p.string() , parDP_bsc , "" , dp_reps );
  double t_ext = time_solver( p.string() , extDP_bsc , "" , dp_reps );
  if( t_std >= 0 ) std::cout << unit << "," << iter << ",stdDP," << t_std << "\n";
  if( t_par >= 0 ) std::cout << unit << "," << iter << ",parDP," << t_par << "\n";
  if( t_ext >= 0 ) std::cout << unit << "," << iter << ",extDP," << t_ext << "\n";

  if( milp_reps > 0 ) {
   double t_milp = time_solver( p.string() , milp_bsc , tpc_bc , milp_reps );
   if( t_milp >= 0 )
    std::cout << unit << "," << iter << ",MILP," << t_milp << "\n";
   else if( ! milp_warned ) {
    std::cerr << "timing_harness: MILP timing skipped (missing config "
                 "or no :MILPSolver available)\n";
    milp_warned = true;
    }
   }
  }
 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*---------------------- End File timing_harness.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
