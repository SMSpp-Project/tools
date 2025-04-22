/*--------------------------------------------------------------------------*/
/*---------------------- File sddp_greedy_solver.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * This is a convenient tool for solving an SDDPBlock using the
 * SDDPGreedySolver. The description of the SDDPBlock must be given in a
 * netCDF file. This tool can be executed as follows:
 *
 *     ./sddp_greedy_solver [-i INDEX] [-b FILE] [-s FILE] <nc4-file>
 *
 * The only mandatory argument is the netCDF containing the description of the
 * SDDPBlock. This netCDF can be either a BlockFile or a ProbFile. The
 * BlockFile can contain any number of child groups, each one describing an
 * SDDPBlock. Every SDDPBlock is then solved. The ProbFile can also contain
 * any number of child groups, each one having the description of an SDDPBlock
 * alongside the description of a BlockConfig and a BlockSolverConfig for the
 * SDDPBlock. Also in this case, every SDDPBlock is solved.
 *
 * The -i option specifies the index of the scenario for which the problem
 * must be solved. The index must be a number between 0 and n-1, where n is
 * the number of scenarios in the SDDPBlock. If this index is not provided,
 * then the problem is solved for the first scenario.
 *
 * The -b and -s options are only considered if the given netCDF file is a
 * BlockFile. The -b option specifies a BlockConfig file to be applied to
 * every SDDPBlock; while the -s option specifies a BlockSolverConfig file for
 * every SDDPBlock. If each of these options is not provided when the given
 * netCDF file is a BlockFile, then default configurations are considered.
 *
 * \author Rafael Durbano Lobato \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Rafael Durbano Lobato
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "common_utils.h"

#include <iostream>
#include <queue>

#include <BendersBlock.h>
#include <HydroSystemUnitBlock.h>
#include <RBlockConfig.h>
#include <SDDPBlock.h>
#include <StochasticBlock.h>
#include <SDDPGreedySolver.h>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

long scenario_id = 0;

/*--------------------------------------------------------------------------*/

const std::string my_short_opts = "i:";

const std::vector< option > my_long_opts = {
  { "scenario" ,                 required_argument , nullptr , 'i' } ,
  { nullptr ,                    no_argument ,       nullptr , 0 }
  };

const std::string my_help =
 "  -i, --scenario <index>          the index of the scenario\n";

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

void process_my_args( int argc , char ** argv )
{
 exe = get_filename( argv[ 0 ] );
 if( argc < 2 ) {
  std::cout << exe << ": no input file\n"
            << "Try " << exe << "' --help' for more information.\n";
  exit( 1 );
  }

 while( true ) {  // options
  auto opt = getopt_long( argc , argv , short_opts.data() ,
			  long_opts.data() , nullptr );
  if( opt == -1 ) break;
  if( process_standard_arg( opt ) )  // if it is a standard one
   continue;                         // next

  switch( opt ) {  // non-standard options
   case 'i': scenario_id = get_long_option();
             if( scenario_id < 0 ) {
	      std::cerr << "scenario index  must be a nonnegative integer"
			<< std::endl;
	      exit( 1 );
	      }
	     break;
   case '?': // Unrecognized option
   default:  std::cerr << "Try " << exe << "' --help' for more information"
		       << std::endl;
             exit( 1 );
   }
  }  // end( while( true ) )

 if( optind < argc )  // last argument == [SDDPBlock] filename
  filename = std::string( argv[ optind ] );
 else {
 std::cout << exe << ": no input file" << std::endl
            << "Try " << exe << "' --help' for more information" << std::endl;
  exit( 1 );
  }
 } // end( process_my_args )

/*--------------------------------------------------------------------------*/

void show_status( Index status , Index fault_stage ) {

 switch( status ) {
  case( SDDPGreedySolver::kError ):
   std::cout << "Error while solving the subproblem at stage "
             << fault_stage << std::endl;
   break;

  case( SDDPGreedySolver::kUnbounded ):
   std::cout << "The subproblem at stage " << fault_stage
             << " is unbounded." << std::endl;
   break;

  case( SDDPGreedySolver::kInfeasible ):
   std::cout << "The problem is infeasible." << std::endl;
   break;

  case( SDDPGreedySolver::kStopTime ):
   std::cout << "A feasible solution has been found. The solution process "
             << "of subproblem at stage " << fault_stage
             << " terminated due a time limit." << std::endl;
   break;

  case( SDDPGreedySolver::kStopIter ):
   std::cout << "A feasible solution has been found. The solution process "
             << "of subproblem at stage " << fault_stage
             << " terminated due an iteration limit." << std::endl;
   break;

  case( SDDPGreedySolver::kLowPrecision ):
   std::cout << "A feasible solution has been found." << std::endl;
   break;

  case( SDDPGreedySolver::kSubproblemInfeasible ):
   std::cout << "The subproblem at stage " << fault_stage
             << " is infeasible." << std::endl;
   break;

  case( SDDPGreedySolver::kSolutionNotFound ):
   std::cout << "A solution for the subproblem at stage "
             << fault_stage << " has not been found." << std::endl;
   break;
  }
 }

/*--------------------------------------------------------------------------*/

void solve( SDDPBlock * sddp_block )
{
 auto solver = dynamic_cast< SDDPGreedySolver * >(
			     sddp_block->get_registered_solvers().front() );
 if( ! solver )
  throw( std::logic_error( "The Solver for the SDDPBlock must be a "
                           "SDDPGreedySolver" ) );

 solver->set_scenario_id( scenario_id );

 if( ! state_in_file.empty() ) {  // load the given State- - - - - - - - - - -
  netCDF::NcFile file;
  try {
   file.open( state_in_file , netCDF::NcFile::read );
   auto state = State::new_State( file );
   solver->put_State( *state );
   delete( state );
   }
  catch( netCDF::exceptions::NcException & e ) {
   std::cout << "Warning: State file " << state_in_file
	     << " could not be loaded" << std::endl;
   }
  catch( const std::exception& e ) {
   std::cout << "Warning: error " << e.what()
	     << " occurred while loading the Solver State" << std::endl;
   }
  }

 // set initial Solution, if provided- - - - - - - - - - - - - - - - - - - - -
 if( ! sol_input.empty() ) {
  if( auto initsol = Solution::deserialize( sol_input ) ) {
   initsol->write( sddp_block );
   delete initsol;
   }
  else
   std::cout << "Warning: input Solution " << sol_input << " invalid"
	     << std::endl;
  }

 if( ! dryrun ) {
  auto status = solver->compute();
  show_status( status , solver->get_fault_stage() );

  auto lb = solver->get_lb();
  auto ub = solver->get_ub();

  std::cout << "Lower bound: " << lb << std::endl;
  std::cout << "Upper bound: " << ub << std::endl;
  }

  // write final Solution, if required - - - - - - - - - - - - - - - - - - - -
  if( ! sol_output.empty() ) {
   netCDF::NcFile f;
   write_open_netCDF( f , sol_output );
   Configuration * outsolcfg = nullptr;
   if( ! sol_cfg_file.empty() )
    if( ! ( outsolcfg = Configuration::deserialize( sol_cfg_file ) ) )
     std::cout << "Warning: output Solution Configuration "
	       << sol_cfg_file << " invalid" << std::endl;

   if( auto sol = sddp_block->get_Solution( outsolcfg , false ) ) {
    sol->serialize( f );
    delete sol;
    }
   else
    std::cout << "Warning: output Solution empty" << std::endl;

   delete outsolcfg;
   }

 if( ! state_out_file.empty() )  // if required, write out State - - - - - - -
  try {
   netCDF::NcFile file( state_out_file , netCDF::NcFile::replace );
   solver->serialize_State( file );
   }
  catch( netCDF::exceptions::NcException & e ) {
   std::cout << "Warning: State file " << state_out_file
	     << " could not be opened" << std::endl;
   }
  catch( const std::exception & e ) {
   std::cout << "Warning: error " << e.what()
	     << " occurred while saving the Solver State" << std::endl;
   }

 }  // end( solve )

/*--------------------------------------------------------------------------*/

void configure_PolyhedralFunctionBlock( SDDPBlock * sddp_block )
{
 for( auto sub_block : sddp_block->get_nested_Blocks() ) {

  auto stochastic_block = static_cast< StochasticBlock * >( sub_block );
  auto benders_block = static_cast< BendersBlock * >(
			    stochastic_block-> get_nested_Blocks().front() );
  auto objective = static_cast< FRealObjective * >(
					    benders_block->get_objective() );
  auto benders_function = static_cast< BendersBFunction * >(
						 objective->get_function() );
  auto inner_block = benders_function->get_inner_block();

  std::queue< Block * > blocks;
  blocks.push( inner_block );

  while( ! blocks.empty() ) {
   auto block = blocks.front();
   blocks.pop();
   auto n = block->get_number_nested_Blocks();
   for( decltype( n ) i = 0 ; i < n ; ++i ) {
    blocks.push( block->get_nested_Block( i ) );
   }

   if( auto polyhedral = dynamic_cast< PolyhedralFunctionBlock * >( block )
       ) {
    auto config = new BlockConfig;
    config->f_static_variables_Configuration =
                                         new SimpleConfiguration< int >( 1 );
    polyhedral->set_BlockConfig( config );
    }
   }
  }
 }

/*--------------------------------------------------------------------------*/

void process_prob_file( const netCDF::NcFile & file )
{
 auto problems = file.getGroups();

 for( auto & problem : problems ) {  // for each problem descriptor:
  auto & problem_group = problem.second;

  // Deserialize block
  auto block_group = problem_group.getGroup( "Block" );
  auto sddp_block = dynamic_cast< SDDPBlock * >(
					  Block::new_Block( block_group ) );
  if( ! sddp_block )
   throw( std::logic_error( "Error while deserializing the SDDPBlock" ) );

  // Configure block
  auto block_config_group = problem_group.getGroup( "BlockConfig" );
  auto block_config = static_cast< BlockConfig * >(
		     BlockConfig::new_Configuration( block_config_group ) );
  if( ! block_config )
   throw( std::logic_error( "BlockConfig group not present" ) );
  block_config->apply( sddp_block );
  block_config->clear();

  // Configure solver
  auto solver_config_group = problem_group.getGroup( "BlockSolver" );
  auto block_solver_config = static_cast< BlockSolverConfig * >(
	       BlockSolverConfig::new_Configuration( solver_config_group ) );
  if( ! block_solver_config )
   throw( std::logic_error( "BlockSolver group not present" ) );
  block_solver_config->apply( sddp_block );
  block_solver_config->clear();

  std::cout << "Problem: " << problem.first << std::endl;

  solve( sddp_block );  // Solve

  // Destroy the Block and the Configurations

  block_config->apply( sddp_block );
  delete( block_config );

  block_solver_config->apply( sddp_block );
  delete( block_solver_config );

  delete( sddp_block );
  }
 }

/*--------------------------------------------------------------------------*/

BlockSolverConfig * build_BlockSolverConfig( void )
{
 auto block_solver_config = new BlockSolverConfig;
 block_solver_config->add_ComputeConfig( "SDDPGreedySolver" );
 return( block_solver_config );
 }

/*--------------------------------------------------------------------------*/

BlockConfig * build_BlockConfig( const SDDPBlock * sddp_block )
{
 // TODO configure all PolyhedralFunctionBlock
 auto sddp_config = new RBlockConfig;
 auto num_stochastic_blocks = sddp_block->get_number_nested_Blocks();

 for( Block::Index index = 0 ; index < num_stochastic_blocks ; ++index ) {
  auto inner_benders_function_solver = new BlockSolverConfig;
  inner_benders_function_solver->add_ComputeConfig( "CPXMILPSolver" );

  auto benders_function_config = new ComputeConfig;
  benders_function_config->f_extra_Configuration =
   new SimpleConfiguration< std::pair< Configuration * , Configuration * > >
   ( std::make_pair< Configuration * , Configuration * >
     ( nullptr , inner_benders_function_solver ) );

  auto stochastic_block_config = new RBlockConfig;
  sddp_config->add_sub_BlockConfig( stochastic_block_config , index );

  auto benders_block_config = new OBlockConfig;
  stochastic_block_config->add_sub_BlockConfig( benders_block_config , 0 );

  benders_block_config->set_Config_Objective( benders_function_config );
  }

 return( sddp_config );
 }

/*--------------------------------------------------------------------------*/

void process_block_file( const netCDF::NcFile & file )
{
 auto blocks = file.getGroups();

 // BlockConfig
 auto given_block_config = get_blockconfig( bconf_file );

 BlockConfig * block_config = nullptr;
 if( given_block_config ) {
  block_config = given_block_config->clone();
  block_config->clear();
  }

 // BlockSolverConfig
 auto solver_config = get_blocksolverconfig( sconf_file );
 if( ! solver_config )
  solver_config = build_BlockSolverConfig();

 auto cleared_solver_config = solver_config->clone();
 cleared_solver_config->clear();

 for( auto block_description : blocks ) {  // for each Block descriptor
  // Deserialize the SDDPBlock
  auto sddp_block = dynamic_cast< SDDPBlock * >(
			    Block::new_Block( block_description.second ) );
  if( ! sddp_block )
   throw( std::logic_error( "Error while deserializing the SDDPBlock." ) );

  // Configure the SDDPBlock
  if( given_block_config )
   given_block_config->apply( sddp_block );
  else {
   configure_PolyhedralFunctionBlock( sddp_block );
   block_config = build_BlockConfig( sddp_block );
   block_config->apply( sddp_block );
   block_config->clear();
   }

  // Configure the Solver
  solver_config->apply( sddp_block );

  solve( sddp_block );  // Solve

  // Destroy the SDDPBlock and the Configurations
  block_config->apply( sddp_block );
  if( ! given_block_config ) {
   delete( block_config );
   block_config = nullptr;
   }

  cleared_solver_config->apply( sddp_block );
  delete( sddp_block );
  }

 delete( block_config );
 delete( given_block_config );
 delete( solver_config );
 delete( cleared_solver_config );
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // append new options to default ones- - - - - - - - - - - - - - - - - - - -
 // note that the last nullprr record in long_opts is overwritten since the
 // new one is further down from there

 docopt_desc = "SMS++ SDDP greedy solver";
 short_opts.append( my_short_opts );
 long_opts.insert( std::prev( long_opts.end() ) ,
		   my_long_opts.begin() , my_long_opts.end() );
 help.append( my_help );

 // process command-line arguments- - - - - - - - - - - - - - - - - - - - - -

 process_my_args( argc , argv );

 // open the file - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 netCDF::NcFile file;
 auto type = read_open_netCDF( file , filename );

 // process the file- - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 switch( type ) {
  case eProbFile: std::cout << filename << " is a problem file, "
			    << "ignoring Block/Solver Configuration(s)..."
			    << std::endl;
                  process_prob_file( file );
		  break;

  case eBlockFile: std::cout << filename << " is a block file" << std::endl;
                   process_block_file( file );
		   break;
  default: std::cerr << filename << " is not a valid SMS++ file" << std::endl;
           exit( 1 );
  }

 return( 0 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*-------------------- End File sddp_greedy_solver.cpp ---------------------*/
/*--------------------------------------------------------------------------*/
