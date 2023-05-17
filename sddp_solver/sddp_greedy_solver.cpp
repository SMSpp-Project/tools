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

#include <getopt.h>
#include <iostream>
#include <queue>

#include <BendersBlock.h>
#include <BlockSolverConfig.h>
#include <CPXMILPSolver.h>
#include <HydroSystemUnitBlock.h>
#include <RBlockConfig.h>
#include <SDDPBlock.h>
#include <StochasticBlock.h>
#include <SDDPGreedySolver.h>

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/

std::string filename{};
std::string block_config_filename{};
std::string solver_config_filename{};
long scenario_id = 0;

/*--------------------------------------------------------------------------*/

void print_help() {
 // http://docopt.org
 std::cout
  << "Usage: sddp_greedy_solver [options] <nc4-file>\n\n"
  << "Options:\n"
  << "  -i <index>, --scenario <index>  The index of the scenario.\n"
  << "  -b <file>,  --blockcfg <file>   Block configuration.\n"
  << "  -s <file>,  --solvercfg <file>  Solver configuration.\n"
  << "  -h, --help                      Print this help." << std::endl;
}

/*--------------------------------------------------------------------------*/

void process_args( int argc , char ** argv ) {

 if( argc < 2 ) {
  print_help();
  exit( 1 );
 }

 const char * const short_opts = "b:s:i:h";
 const option long_opts[] = {
  { "blockcfg" ,  required_argument , nullptr , 'b' } ,
  { "solvercfg" , required_argument , nullptr , 's' } ,
  { "scenario" ,  required_argument , nullptr , 'i' } ,
  { "help" ,      no_argument ,       nullptr , 'h' } ,
  { nullptr ,     no_argument ,       nullptr , 0 }
 };

 // Options
 while( true ) {
  const auto opt = getopt_long( argc , argv , short_opts ,
                                long_opts , nullptr );

  if( opt == -1 ) {
   break;
  }

  switch( opt ) {
   case 'b':
    block_config_filename = std::string( optarg );
    break;
   case 's':
    solver_config_filename = std::string( optarg );
    break;
   case 'i': {
    char * end = nullptr;
    errno = 0;
    scenario_id = std::strtol( optarg , &end , 10 );

    if( ( ! optarg ) || ( ( scenario_id = std::strtol( optarg , &end , 10 ) ) ,
                          ( errno || ( end && *end ) ) ) ||
        ( scenario_id < 0 ) ) {
     std::cout << "The index of the scenario must be a nonnegative integer."
               << std::endl;
     exit( 1 );
    }
    break;
   }
   case 'h': // -h or --help
    print_help();
    exit( 0 );
   case '?': // Unrecognized option
   default:
    print_help();
    exit( 1 );
  }
 }

 // Last argument
 if( optind < argc ) {
  filename = std::string( argv[ optind ] );
 }
 else {
  print_help();
  exit( 1 );
 }
}

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

void solve( SDDPBlock * sddp_block ) {

 auto solver = dynamic_cast< SDDPGreedySolver * >
  ( sddp_block->get_registered_solvers().front() );

 if( ! solver )
  throw( std::logic_error( "The Solver for the SDDPBlock must be a "
                           "SDDPGreedySolver." ) );

 solver->set_scenario_id( scenario_id );

 auto status = solver->compute();

 show_status( status , solver->get_fault_stage() );

 auto lb = solver->get_lb();
 auto ub = solver->get_ub();

 std::cout << "Lower bound: " << lb << std::endl;
 std::cout << "Upper bound: " << ub << std::endl;
}

/*--------------------------------------------------------------------------*/

void configure_PolyhedralFunctionBlock( SDDPBlock * sddp_block ) {
 for( auto sub_block : sddp_block->get_nested_Blocks() ) {

  auto stochastic_block = static_cast< StochasticBlock * >( sub_block );
  auto benders_block = static_cast< BendersBlock * >
   ( stochastic_block-> get_nested_Blocks().front() );
  auto objective = static_cast< FRealObjective * >
   ( benders_block->get_objective() );
  auto benders_function = static_cast< BendersBFunction * >
   ( objective->get_function() );
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

   if( auto polyhedral = dynamic_cast< PolyhedralFunctionBlock * >( block ) ) {
    auto config = new BlockConfig;
    config->f_static_variables_Configuration = new SimpleConfiguration< int >(1);
    polyhedral->set_BlockConfig( config );
   }
  }
 }
}

/*--------------------------------------------------------------------------*/

void process_prob_file( const netCDF::NcFile & file ) {
 std::multimap< std::string , netCDF::NcGroup > problems = file.getGroups();
 // for each problem descriptor:
 for( auto & problem : problems ) {

  auto & problem_group = problem.second;

  // Deserialize block
  auto block_group = problem_group.getGroup( "Block" );
  auto sddp_block = dynamic_cast< SDDPBlock * >( Block::new_Block( block_group ) );
  if( ! sddp_block )
   throw( std::logic_error( "Error while deserializing the SDDPBlock." ) );

  // Configure block
  auto block_config_group = problem_group.getGroup( "BlockConfig" );
  auto block_config = static_cast< BlockConfig * >
   ( BlockConfig::new_Configuration( block_config_group ) );
  if( ! block_config )
   throw( std::logic_error("BlockConfig group was not properly provided.") );
  block_config->apply( sddp_block );
  block_config->clear();

  // Configure solver
  auto solver_config_group = problem_group.getGroup( "BlockSolver" );
  auto block_solver_config = static_cast< BlockSolverConfig * >
   ( BlockSolverConfig::new_Configuration( solver_config_group ) );
  if( ! block_solver_config )
   throw( std::logic_error("BlockSolver group was not properly provided.") );
  block_solver_config->apply( sddp_block );
  block_solver_config->clear();

  std::cout << "Problem: " << problem.first << std::endl;

  // Solve

  solve( sddp_block );

  // Destroy the Block and the Configurations

  block_config->apply( sddp_block );
  delete( block_config );

  block_solver_config->apply( sddp_block );
  delete( block_solver_config );

  delete( sddp_block );
 }
}

/*--------------------------------------------------------------------------*/

BlockSolverConfig * build_BlockSolverConfig() {
 auto block_solver_config = new BlockSolverConfig;
 block_solver_config->add_ComputeConfig( "SDDPGreedySolver" );
 return( block_solver_config );
}

/*--------------------------------------------------------------------------*/

BlockConfig * build_BlockConfig( const SDDPBlock * sddp_block ) {
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

BlockConfig * load_BlockConfig() {
 BlockConfig * block_config = nullptr;
 std::ifstream block_config_file;
 block_config_file.open( block_config_filename , std::ifstream::in );

 if( block_config_file.is_open() ) {
  std::cout << "Using Block configuration in " << block_config_filename
            << "." << std::endl;

  std::string config_name;
  block_config_file >> eatcomments >> config_name;
  block_config = dynamic_cast< BlockConfig * >
   ( Configuration::new_Configuration( config_name ) );

  if( ! block_config ) {
   std::cerr << "Block configuration is not valid: "
             << config_name << std::endl;
   exit( 1 );
  }

  try {
   block_config_file >> *block_config;
  }
  catch( const std::exception& e ) {
   std::cerr << "Block configuration is not valid: " << e.what() << std::endl;
   exit( 1 );
  }
 }
 else {
  std::cout << "Block configuration was not provided. "
   "Using default configuration." << std::endl;
 }
 return( block_config );
}

/*--------------------------------------------------------------------------*/

BlockSolverConfig * load_BlockSolverConfig() {
 BlockSolverConfig * solver_config = nullptr;
 std::ifstream solver_config_file;
 solver_config_file.open( solver_config_filename , std::ifstream::in );

 if( solver_config_file.is_open() ) {
  std::cout << "Using Solver configuration in " << solver_config_filename
            << "." << std::endl;

  std::string config_name;
  solver_config_file >> eatcomments >> config_name;
  solver_config = dynamic_cast< BlockSolverConfig * >
   ( Configuration::new_Configuration( config_name ) );

  if( ! solver_config ) {
   std::cerr << "Solver configuration is not valid: " << config_name << std::endl;
   exit( 1 );
  }

  try {
   solver_config_file >> *solver_config;
  }
  catch( ... ) {
   std::cout << "Solver configuration is not valid." << std::endl;
   exit( 1 );
  }
 }
 else {
  std::cout << "Solver configuration was not provided. "
   "Using default configuration." << std::endl;
 }
 return( solver_config );
}

/*--------------------------------------------------------------------------*/

void process_block_file( const netCDF::NcFile & file ) {
 std::multimap< std::string , netCDF::NcGroup > blocks = file.getGroups();

 // BlockConfig
 auto given_block_config = load_BlockConfig();

 BlockConfig * block_config = nullptr;
 if( given_block_config ) {
  block_config = given_block_config->clone();
  block_config->clear();
 }

 // BlockSolverConfig
 auto solver_config = load_BlockSolverConfig();
 if( ! solver_config )
  solver_config = build_BlockSolverConfig();

 auto cleared_solver_config = solver_config->clone();
 cleared_solver_config->clear();

 // For each Block descriptor
 for( auto block_description : blocks ) {

  // Deserialize the SDDPBlock

  auto sddp_block = dynamic_cast< SDDPBlock * >
   ( Block::new_Block( block_description.second ) );

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

  // Solve

  solve( sddp_block );

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

int main( int argc , char ** argv ) {

 process_args( argc , argv );

 netCDF::NcFile file;
 try {
  file.open( filename , netCDF::NcFile::read );
 } catch( netCDF::exceptions::NcException & e ) {
  std::cerr << "Cannot open nc4 file " << filename << std::endl;
  exit( 1 );
 }

 netCDF::NcGroupAtt gtype = file.getAtt( "SMS++_file_type" );
 if( gtype.isNull() ) {
  std::cerr << filename << " is not an SMS++ nc4 file." << std::endl;
  exit( 1 );
 }

 int type;
 gtype.getValues( &type );

 switch( type ) {
  case eProbFile: {
   std::cout << filename << " is a problem file, "
    "ignoring Block/Solver configurations..." << std::endl;
   process_prob_file( file );
   break;
  }

  case eBlockFile: {
   std::cout << filename << " is a block file." << std::endl;
   process_block_file( file );
   break;
  }

  default:
   std::cerr << filename << " is not a valid SMS++ file." << std::endl;
   exit( 1 );
 }

 return( 0 );
}
