/*--------------------------------------------------------------------------*/
/*-------------------------- File tssb_solver.cpp --------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * This is a convenient tool for solving a TwoStageStochasticBlock.
 * The description of the TwoStageStochasticBlock must be given in a netCDF
 * file. This tool can be executed as follows:
 *
 *   ./tssb_solver [-s] [-e] [-m NUMBER] [-B FILE] [-S FILE] [-p PATH]
 *                 [-c PATH] [-k] < nc4-file >
 *
 * The only mandatory argument is the netCDF file containing the description
 * of the TwoStageStochasticBlock. This can be either a BlockFile or
 * a ProbFile. The BlockFile can contain any number of child groups, each one
 * describing a TwoStageStochasticBlock, each of which is then solved with
 * the same BlockConfig and BlockSolverConfig. The ProbFile can also contain
 * any number of child groups, each one having the description of a
 * TwoStageStochasticBlock alongside the description of a BlockConfig and a
 * BlockSolverConfig for the TwoStageStochasticBlock; thus, every
 * TwoStageStochasticBlock is solved with these specified BlockConfig and
 * BlockSolverConfig.
 *
 * The -c option specifies the prefix to the paths to all configuration
 * files. This means that if PATH is the value passed to the -c option, then
 * the name (or path) to each configuration file will be prepended by
 * PATH. The -p option specifies the prefix to the paths to all files
 * specified by the attribute "filename" in the input netCDF file.
 *
 * The -B and -S options are only considered if the given netCDF file is a
 * BlockFile. The -B option specifies a BlockConfig file to be applied to
 * every TwoStageStochasticBlock; while the -S option specifies a
 * BlockSolverConfig file for every TwoStageStochasticBlock. If each of these
 * options is not provided when the given netCDF file is a BlockFile, then
 * default configurations are considered.
 *
 * The -k option solves the Benders form of each TwoStageStochasticBlock
 * rather than the TwoStageStochasticBlock itself [see
 * TwoStageStochasticBlock::get_Benders_form()]: the BlockConfig is applied
 * to the TwoStageStochasticBlock, the Benders form is assembled around it,
 * and the BlockSolverConfig is applied to the root of the form, which is
 * where a Benders decomposition Solver is attached. The form is given back
 * once solved. It is only available for a BlockFile.
 *
 * The -R option recovers a feasible solution after a Solver that only gives
 * a bound, e.g., a LagrangianDualSolver relaxing the non-anticipativity
 * Constraint: the here-and-now Variable of every leaf are fixed to their
 * mean over the leaves, rounded where they are integer, each leaf is solved
 * alone with the BlockSolverConfig given to -R, and the sum of their values
 * is the value of a feasible solution, printed with the gap to the bound.
 * The primal solution the Solver gives, if any, is written first, so that
 * the mean is taken on it. It is only available for a BlockFile.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni, Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <iomanip>
#include <iostream>

#include <AbstractBlock.h>

#include <BlockSolverConfig.h>

#include <chrono>

#include <cmath>

#include <TwoStageStochasticBlock.h>

#include "common_utils.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

bool benders_form = false;  ///< solve the Benders form (-k)

std::string recover_sconf;  ///< BlockSolverConfig of the leaves (-R)

const std::string my_short_opts = "kR:";

const std::vector< option > my_long_opts = {
  { "benders" , no_argument , nullptr , 'k' } ,
  { "recover" , required_argument , nullptr , 'R' }
  };

const std::string my_help =
 "  -k, --benders                   solve the Benders form of the problem,\n"
 "                                  the Solver being attached to its root\n"
 "  -R, --recover <file>            recover a feasible solution by fixing\n"
 "                                  the here-and-now Variable to their mean\n"
 "                                  over the leaves and solving each leaf\n"
 "                                  with the BlockSolverConfig in <file>";

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 switch( opt ) {  // non-standard options
  case 'k': benders_form = true; return( true );
  case 'R': recover_sconf = std::string( optarg ); return( true );
  default: return( false );
  }
 }

/*--------------------------------------------------------------------------*/

void process_prob_file( const netCDF::NcFile & file )
{
 if( benders_form ) {
  std::cout << "Error: the Benders form (-k) needs a Block file, whose "
               "BlockSolverConfig is given by -S" << std::endl;
  exit( 1 );
  }

 auto problems = file.getGroups();

 for( auto & problem : problems ) {  // for each problem descriptor:
  Block * block;
  Configuration * s_config;
  get_all( problem.second , block , s_config );

  if( ! dynamic_cast< TwoStageStochasticBlock * >( block ) ) {
   std::cout << "Error: " << problem.first
	     << " not a TwoStageStochasticBlock" << std::endl;
   exit( 1 );
   }

  std::cout << "Problem: " << problem.first << std::endl;

  set_solver_logs( block );

  // Solve
  solve_all( block );

  // cleanup
  cleanup_bsc( block , s_config );
  delete s_config;
  delete block;
  }
 }

/*--------------------------------------------------------------------------*/

/// solves the Benders form of the TwoStageStochasticBlock in \p group
/** The BlockConfig is applied to the TwoStageStochasticBlock, whose abstract
 * representation is then generated, since the form is read off it; the
 * BlockSolverConfig is applied to the root of the form. */

static void solve_Benders_form( const std::string & name ,
                                const netCDF::NcGroup & group )
{
 require_solver_config( sconf_file );
 auto block = get_Block( group );
 auto tssb = dynamic_cast< TwoStageStochasticBlock * >( block );
 if( ! tssb ) {
  std::cout << "Error: " << name << " not a TwoStageStochasticBlock"
            << std::endl;
  exit( 1 );
  }

 auto b_config = get_config( bconf_file );
 config_Block( block , b_config , nullptr );
 delete b_config;

 tssb->generate_abstract_variables();
 tssb->generate_abstract_constraints();
 tssb->generate_objective();

 auto form = tssb->get_Benders_form();
 if( ! form ) {
  std::cout << "Error: " << name << " declares no here-and-now Variable, "
               "hence it has no Benders form" << std::endl;
  exit( 1 );
  }

 auto s_config = get_config( sconf_file );
 config_Block( form , nullptr , s_config );

 set_solver_logs( form );

 // Solve
 solve_all( form );

 // cleanup
 cleanup_bsc( form , s_config );
 delete s_config;
 tssb->give_back_Benders_form( form );
 delete block;
 }

/*--------------------------------------------------------------------------*/

/// recovers a feasible solution of \p tssb out of the bound \p lb
/** The here-and-now Variable of every leaf are fixed to their mean over the
 * leaves, rounded where they are integer; each leaf, whose Objective already
 * carries the probability of its scenario, is then solved alone with the
 * BlockSolverConfig of -R, and the sum of their values is the value of a
 * feasible solution of the two-stage problem. The Variable are unfixed and
 * the Solver detached at the end. */

static void recover_primal( TwoStageStochasticBlock * tssb , double lb )
{
 using Index = Block::Index;
 const auto start = std::chrono::system_clock::now();

 const Index L = tssb->get_number_leaves();
 std::vector< std::vector< ColVariable * > > xk( L );
 for( Index l = 0 ; l < L ; ++l )
  for( const auto & p : tssb->get_paths_to_static_here_and_now_vars() ) {
   auto leaf = tssb->get_leaf_block( l );
   const auto nv = p->get_number_elements< ColVariable >( leaf );
   auto e = p->get_element< ColVariable >( leaf );
   for( Index j = 0 ; j < nv ; ++j )
    xk[ l ].push_back( e + j );
   }

 const Index n = L ? xk[ 0 ].size() : 0;
 if( ! n ) {
  std::cout << "Recovered primal = none (no here-and-now Variable)"
            << std::endl;
  return;
  }
 std::vector< double > mean( n , 0 );
 for( Index l = 0 ; l < L ; ++l )
  for( Index j = 0 ; j < n ; ++j )
   mean[ j ] += xk[ l ][ j ]->get_value() / L;
 for( Index j = 0 ; j < n ; ++j )
  if( xk[ 0 ][ j ]->is_integer() )
   mean[ j ] = std::round( mean[ j ] );

 auto bsc = dynamic_cast< BlockSolverConfig * >( get_config( recover_sconf ) );
 if( ! bsc ) {
  std::cout << "Error: " << recover_sconf << " is not a BlockSolverConfig"
            << std::endl;
  exit( 1 );
  }

 double ub = 0;
 bool feasible = true;
 for( Index l = 0 ; ( l < L ) && feasible ; ++l ) {
  auto leaf = tssb->get_leaf_block( l );
  std::vector< bool > was_fixed( n );
  for( Index j = 0 ; j < n ; ++j ) {
   was_fixed[ j ] = xk[ l ][ j ]->is_fixed();
   xk[ l ][ j ]->set_value( mean[ j ] );
   xk[ l ][ j ]->is_fixed( true , eNoMod );
   }

  // a copy per leaf, clear()-ing it being what detaches its Solver
  auto lbsc = bsc->clone();
  lbsc->apply( leaf );
  if( leaf->get_registered_solvers().empty() ) {
   std::cout << "Error: " << recover_sconf << " attaches no Solver to leaf "
             << l << std::endl;
   exit( 1 );
   }
  auto solver = leaf->get_registered_solvers().front();
  const auto status = solver->compute();
  if( ( status == Solver::kOK ) || ( status == Solver::kLowPrecision ) )
   ub += solver->get_ub();
  else
   feasible = false;

  lbsc->clear();
  lbsc->apply( leaf );
  delete lbsc;
  for( Index j = 0 ; j < n ; ++j )
   if( ! was_fixed[ j ] )
    xk[ l ][ j ]->is_fixed( false , eNoMod );
  }

 delete bsc;

 const std::chrono::duration< double > t =
                                     std::chrono::system_clock::now() - start;
 std::cout << std::setprecision( 10 );
 std::cout << "Recovery time: " << t.count() << " s" << std::endl;
 if( ! feasible ) {
  std::cout << "Recovered primal = none (a leaf is not solved)" << std::endl;
  return;
  }

 std::cout << "Recovered primal = " << ub << std::endl;
 std::cout << "Gap to the bound = " << ( ub - lb ) / std::abs( ub )
           << std::endl;
 }

/*--------------------------------------------------------------------------*/

void process_block_file( const netCDF::NcFile & file )
{
 auto blocks = file.getGroups();

 for( auto & b : blocks ) {  // for each Block descriptor
  if( benders_form ) {
   solve_Benders_form( b.first , b.second );
   continue;
   }

  Block * block;
  Configuration * s_config;
  get_all( b.second , bconf_file , sconf_file , block , s_config );

  if( ! dynamic_cast< TwoStageStochasticBlock * >( block ) ) {
   std::cout << "Error: " << b.first
	     << " not a TwoStageStochasticBlock" << std::endl;
   exit( 1 );
   }

  set_solver_logs( block );

  // Solve
  solve_all( block );

  // the bound, and the primal solution the Solver gives, if any, which the
  // recovery takes the mean on
  double lb = - Inf< double >();
  if( ! recover_sconf.empty() )
   for( auto solver : block->get_registered_solvers() ) {
    lb = std::max( lb , solver->get_lb() );
    if( solver->has_var_solution() )
     solver->get_var_solution();
    }

  // cleanup
  cleanup_bsc( block , s_config );

  if( ! recover_sconf.empty() )
   recover_primal( static_cast< TwoStageStochasticBlock * >( block ) , lb );

  delete s_config;
  delete block;
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // append new options to default ones- - - - - - - - - - - - - - - - - - - -
 // note that the last nullptr record in long_opts is overwritten since the
 // new one is further down from there

 docopt_desc =
  "SMS++ TSSB solver: loads a two-stage stochastic problem (a\n"
  "TwoStageStochasticBlock) and solves it with the Solvers of its\n"
  "BlockSolverConfig.\n";
 docopt_args =
  "  <file>    SMS++ netCDF file (.nc4) holding a TwoStageStochasticBlock:\n"
  "            a Block file, or a problem file, whose own configuration is\n"
  "            then used and -B and -S are ignored\n";
 docopt_examples =
  "  tssb_solver instance.nc4\n"
  "      solve the deterministic equivalent with a :MILPSolver\n"
  "  tssb_solver -S TSSBSCfg-LD.txt instance.nc4\n"
  "      solve the Lagrangian dual of the scenario decomposition, whose\n"
  "      master problem needs CPLEX or Gurobi\n"
  "  tssb_solver -k -S TSSBSCfg-BDS.txt instance.nc4\n"
  "      solve the Benders form of the problem with the\n"
  "      BendersDecompositionSolver of TSSBSCfg-BDS.txt\n"
  "  tssb_solver -c myconfig/ instance.nc4\n"
  "      use the Configuration files in myconfig/, e.g. a modified copy\n"
  "      of the installed ones\n";

 // Configuration files live in config/ by default; an explicit -c overrides
 // this. Default -B / -S so a plain run needs neither: TSSBCfg.txt is the
 // BlockConfig (anchor/sequential formulation) and TSSBSCfg.txt the
 // BlockSolverConfig for the TwoStageStochasticBlock
 conf_prefix = "config/";
 default_bconf_name = "TSSBCfg.txt";
 default_sconf_name = "TSSBSCfg.txt";

 // process command-line arguments- - - - - - - - - - - - - - - - - - - - - -

 short_opts.append( my_short_opts );
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_long_opts.begin() , my_long_opts.end() );
 help.append( my_help );

 process_args( argc , argv , process_specific_arg );

 // open the file - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 netCDF::NcFile file;
 auto type = read_open_netCDF( file , filename );

 // process the file- - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 switch( type ) {
  case( eProbFile ):
   std::cout << filename << " is a problem file, "
	     << "ignoring Block/Solver Configuration(s)..." << std::endl;
   process_prob_file( file );
   break;
  case( eBlockFile ):
   std::cout << filename << " is a Block file" << std::endl;
   process_block_file( file );
   break;
  default :
   std::cerr << filename << " is not a valid SMS++ file" << std::endl;
   exit( 1 );
  }

 return( 0 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File tssb_solver.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
