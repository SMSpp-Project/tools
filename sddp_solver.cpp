/*--------------------------------------------------------------------------*/
/*-------------------------- File sddp_solver.cpp --------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * This is a convenient tool for solving an SDDPBlock using either the
 * SDDPSolver or the SDDPGreedySolver. The description of the SDDPBlock must
 * be given in a netCDF file. This tool can be executed as follows:
 *
 *   ./sddp_solver [-s] [-i INDEX] [-r] [-B FILE] [-S FILE] [-p PATH] [-c PATH] <nc4-file>
 *
 * The only mandatory argument is the netCDF containing the description of the
 * SDDPBlock. This netCDF can be either a BlockFile or a ProbFile. The
 * BlockFile can contain any number of child groups, each one describing an
 * SDDPBlock. Every SDDPBlock is then solved. The ProbFile can also contain
 * any number of child groups, each one having the description of an SDDPBlock
 * alongside the description of a BlockConfig and a BlockSolverConfig for the
 * SDDPBlock. Also in this case, every SDDPBlock is solved.
 *
 * The -c option specifies the prefix to the paths to all configuration
 * files. The -p option specifies the prefix to the paths to all files
 * specified by the attribute "filename" in the input netCDF file.
 *
 * The -s option indicates whether a simulation should be performed. If this
 * option is used, then the SDDPBlock is solved using the
 * SDDPGreedySolver. Otherwise, the SDDPBlock is solved by the SDDPSolver.
 *
 * In simulation mode (i.e., when the -s option is used), the -i option
 * specifies the index of the scenario for which the problem must be
 * solved. The index must be a number between 0 and n-1, where n is the number
 * of scenarios in the SDDPBlock. If this index is not provided, then the
 * problem is solved for the first scenario. Also in simulation mode, the -r
 * option indicates that the integrality constraints over the variables must
 * be relaxed.
 *
 * The -B and -S options are only considered if the given netCDF file is a
 * BlockFile. The -B option specifies a BlockConfig file to be applied to
 * every SDDPBlock; while the -S option specifies a BlockSolverConfig file for
 * every SDDPBlock. If each of these options is not provided when the given
 * netCDF file is a BlockFile, then default configurations are considered.
 *
 * \version 0.1
 *
 * \date 22 - 01 - 2021
 *
 * \author Rafael Durbano Lobato \n
 *         Operations Research Group \n
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
#include <SDDPSolver.h>

#include "CutProcessing.h"
#include "SDDPBlockSolutionOutput.h"

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/

std::string filename{};
std::string block_config_filename{};
std::string solver_config_filename{};
long scenario_id = 0;
bool simulation_mode = false;
bool relax_integrality = false;
const bool continuous_relaxation = true;

std::string exe{};         ///< Name of the executable file
std::string docopt_desc{}; ///< Tool description

/*--------------------------------------------------------------------------*/

// Gets the name of the executable from its full path
std::string get_filename( const std::string & fullpath ) {
 std::size_t found = fullpath.find_last_of( "/\\" );
 return fullpath.substr( found + 1 );
}

/*--------------------------------------------------------------------------*/

void print_help() {
 // http://docopt.org
 std::cout << docopt_desc << std::endl;
 std::cout << "Usage:\n"
           << "  " << exe << " [options] <file>\n"
           << "  " << exe << " -h | --help\n"
           << std::endl
           << "Options:\n"
           << "  -B, --blockcfg <file>   Block configuration.\n"
           << "  -c, --configdir <path>  The prefix for all config filenames.\n"
           << "  -h, --help              Print this help.\n"
           << "  -i, --scenario <index>  The index of the scenario.\n"
           << "  -p, --prefix <path>     The prefix for all Block filenames.\n"
           << "  -r, --relax             Relax integer variables.\n"
           << "  -s, --simulation        Simulation mode.\n"
           << "  -S, --solvercfg <file>  Solver configuration."
           << std::endl;
}

/*--------------------------------------------------------------------------*/

void process_args( int argc , char ** argv ) {

 if( argc < 2 ) {
  std::cout << exe << ": no input file\n"
            << "Try " << exe << "' --help' for more information.\n";
  exit( 1 );
 }

 const char * const short_opts = "B:c:hi:p:rsS:";
 const option long_opts[] = {
  { "blockcfg" ,   required_argument , nullptr , 'B' } ,
  { "configdir" ,  required_argument , nullptr , 'c' } ,
  { "help" ,       no_argument ,       nullptr , 'h' } ,
  { "scenario" ,   required_argument , nullptr , 'i' } ,
  { "prefix" ,     required_argument , nullptr , 'p' } ,
  { "relax" ,      no_argument ,       nullptr , 'r' } ,
  { "simulation" , no_argument ,       nullptr , 's' } ,
  { "solvercfg" ,  required_argument , nullptr , 'S' } ,
  { nullptr ,      no_argument ,       nullptr , 0 }
 };

 // Options
 while( true ) {
  const auto opt = getopt_long( argc , argv , short_opts ,
                                long_opts , nullptr );

  if( opt == -1 ) {
   break;
  }

  switch( opt ) {
   case 'B':
    block_config_filename = std::string( optarg );
    break;
   case 'c':
    Configuration::set_filename_prefix( std::string( optarg ) );
    break;
   case 'S':
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
   case 'p':
    Block::set_filename_prefix( std::string( optarg ) );
    break;
   case 's':
    simulation_mode = true;
    break;
   case 'r':
    relax_integrality = true;
    break;
   case 'h': // -h or --help
    print_help();
    exit( 0 );
   case '?': // Unrecognized option
   default:
    std::cout << "Try " << exe << "' --help' for more information.\n";
    exit( 1 );
  }
 }

 // Last argument
 if( optind < argc ) {
  filename = std::string( argv[ optind ] );
 }
 else {
  std::cout << exe << ": no input file\n"
            << "Try " << exe << "' --help' for more information.\n";
  exit( 1 );
 }
}

/*--------------------------------------------------------------------------*/

void show_simulation_status( Index status , Index fault_stage ) {

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

Block * get_uc_block( const SDDPBlock * sddp_block , const Index stage ) {

 auto benders_block = static_cast< BendersBlock * >
  ( sddp_block->get_sub_Block( stage )->get_inner_block() );

 auto objective = static_cast< FRealObjective * >
  ( benders_block->get_objective() );

 auto benders_function = static_cast< BendersBFunction * >
  ( objective->get_function() );

 return benders_function->get_inner_block();
}

/*--------------------------------------------------------------------------*/

bool update_hydro_unit( Block * previous_block , Block * block ,
                        const Index stage ) {
 auto unit = dynamic_cast< HydroUnitBlock * >( block );
 auto previous_unit = dynamic_cast< HydroUnitBlock * >( previous_block );

 if( ! unit && ! previous_unit )
  return false;

 if( ! unit || ! previous_unit )
  throw( std::logic_error
         ( "sddp_solver: UCBlocks at stages " + std::to_string( stage - 1 ) +
           " and " + std::to_string( stage ) +
           " do not have the same structure." ) );

 auto number_generators = previous_unit->get_number_generators();

 if( number_generators != unit->get_number_generators() )
  throw( std::logic_error
         ( "sddp_solver: HydroUnitBlock at stage " +
           std::to_string( stage - 1 ) + " has " +
           std::to_string( number_generators ) +
           ", but corresponding HydroUnitBlock at stage " +
           std::to_string( stage ) + " has " +
           std::to_string( unit->get_number_generators() ) ) );

 std::vector< double > flow_rate( number_generators );

 for( Index g = 0 ; g < number_generators ; ++g )
  flow_rate[ g ] = previous_unit->get_flow_rate( g )->get_value();

 unit->set_initial_flow_rate( flow_rate.cbegin() );

 return true;
}

/*--------------------------------------------------------------------------*/

bool update_battery_unit( Block * previous_block , Block * block ,
                          const Index stage ) {
 auto unit = dynamic_cast< BatteryUnitBlock * >( block );
 auto previous_unit = dynamic_cast< BatteryUnitBlock * >( previous_block );

 if( ! unit && ! previous_unit )
  return false;

 if( ! unit || ! previous_unit )
  throw( std::logic_error
         ( "sddp_solver: UCBlocks at stages " + std::to_string( stage - 1 ) +
           " and " + std::to_string( stage ) +
           " do not have the same structure." ) );

 const auto time_horizon = unit->get_time_horizon();

 std::vector< double > initial_power_data =
  { ( previous_unit->get_active_power( 0 ) + time_horizon - 1 )->get_value() };

 unit->set_initial_power( initial_power_data.cbegin() );

 std::vector< double > initial_storage_data =
  { previous_unit->get_storage_level()[ time_horizon - 1 ].get_value() };

 unit->set_initial_storage( initial_storage_data.cbegin() );

 return true;
}

/*--------------------------------------------------------------------------*/

int compute_init_up_down_time( const SDDPBlock * sddp_block ,
                               ThermalUnitBlock * previous_unit ,
                               ThermalUnitBlock * unit , const Index stage ) {

 auto time_horizon = previous_unit->get_time_horizon();
 auto commitment = previous_unit->get_commitment( 0 ) + time_horizon - 1;

 auto shutdown = previous_unit->get_shut_down();
 if( ! shutdown.empty() && shutdown.back().get_value() >= 0.5 ) {
  return 0;
 }

 int init_up_down_time = 0;
 const bool on = commitment->get_value() >= 0.5;
 if( on )
  init_up_down_time = 1;
 else
  init_up_down_time = -1;

 AbstractPath path;

 for( Index outer_t = 0 ; outer_t < stage ; ++outer_t ) {

  for( Index t = 1 ; t < time_horizon ; ++t , --commitment ) {
   if( std::abs( commitment->get_value() -
                 ( commitment - 1 )->get_value() ) > 0.5 )
    return init_up_down_time;
   if( on ) ++init_up_down_time;
   else --init_up_down_time;
  }

  if( outer_t == stage - 1 )
   break;

  if( path.empty() ) {
   auto uc_block = get_uc_block( sddp_block , stage );
   path.build( unit , uc_block );
  }

  auto previous_uc_block = get_uc_block( sddp_block , stage - outer_t - 2 );
  previous_unit = dynamic_cast< ThermalUnitBlock * >
   ( path.get_element< Block >( previous_uc_block ) );

  if( ! previous_unit )
   throw( std::logic_error
          ( "sddp_solver::update_thermal_block: ThermalUnitBlock not found "
            "at stage " + std::to_string( stage - outer_t - 2 ) ) );

  commitment = previous_unit->get_commitment( 0 ) + time_horizon - 1;

  if( on ) {
   if( commitment->get_value() >= 0.5 ) ++init_up_down_time;
   else break;
  }
  else {
   if( commitment->get_value() < 0.5 ) --init_up_down_time;
   else break;
  }
 }

 return init_up_down_time;
}

/*--------------------------------------------------------------------------*/

bool update_thermal_unit( const SDDPBlock * sddp_block ,
                          Block * previous_block , Block * block ,
                          const Index stage ) {

 auto previous_unit = dynamic_cast< ThermalUnitBlock * >( previous_block );
 auto unit = dynamic_cast< ThermalUnitBlock * >( block );

 if( ! unit && ! previous_unit )
  return false;

 if( ! unit || ! previous_unit )
  throw( std::logic_error
         ( "sddp_solver: UCBlocks at stages " + std::to_string( stage - 1 ) +
           " and " + std::to_string( stage ) +
           " do not have the same structure." ) );

 auto init_up_down_time = compute_init_up_down_time
  ( sddp_block , previous_unit , unit , stage );

 std::vector< int > init_up_down_time_data = { init_up_down_time };
 unit->set_init_updown_time( init_up_down_time_data.cbegin() );

 std::vector< double > active_power_data =
  { previous_unit->get_active_power( 0 )->get_value() };
 unit->set_initial_power( active_power_data.cbegin() );

 return true;
}

/*--------------------------------------------------------------------------*/

void callback( SDDPBlock * sddp_block , Block::Index stage ) {

 if( stage == 0 )
  return;

 auto previous_uc_block = get_uc_block( sddp_block , stage - 1 );
 auto uc_block = get_uc_block( sddp_block , stage );

 std::queue< Block *> blocks;
 blocks.push( uc_block );

 std::queue< Block *> previous_blocks;
 previous_blocks.push( previous_uc_block );

 while( ! blocks.empty() ) {
  auto block = blocks.front();
  blocks.pop();

  auto previous_block = previous_blocks.front();
  previous_blocks.pop();

  auto n = block->get_number_nested_Blocks();

  if( n != previous_block->get_number_nested_Blocks() ) {
   throw( std::logic_error
          ("sddp_solver: UCBlocks at stages " + std::to_string( stage - 1 ) +
           " and " + std::to_string( stage ) +
           " do not have the same structure." ) );
  }

  for( decltype( n ) i = 0 ; i < n ; ++i ) {
   blocks.push( block->get_nested_Block( i ) );
   previous_blocks.push( previous_block->get_nested_Block( i ) );
  }

  update_hydro_unit( previous_block , block , stage )
   || update_thermal_unit( sddp_block , previous_block , block , stage )
   || update_battery_unit( previous_block , block , stage );
 }
}

/*--------------------------------------------------------------------------*/

void simulate( SDDPBlock * sddp_block ) {

 auto solver = dynamic_cast< SDDPGreedySolver * >
  ( sddp_block->get_registered_solvers().front() );

 if( ! solver )
  throw( std::logic_error( "The Solver for the SDDPBlock must be a "
                           "SDDPGreedySolver." ) );

 solver->set_callback( [sddp_block]( Index stage ) {
  callback( sddp_block , stage );
 });

 solver->set_scenario_id( scenario_id );

 auto status = solver->compute();

 show_simulation_status( status , solver->get_fault_stage() );

 if( solver->has_var_solution() ) {
  solver->get_var_solution();
  SDDPBlockSolutionOutput output;
  output.print( sddp_block );
 }

 auto lb = solver->get_lb();
 auto ub = solver->get_ub();

 std::cout << "Lower bound: " << std::setprecision( 20 ) << lb << std::endl;
 std::cout << "Upper bound: " << std::setprecision( 20 ) << ub << std::endl;
}

/*--------------------------------------------------------------------------*/

void show_status( Index status ) {

 switch( status ) {

  case( SDDPSolver::kError ):
   std::cout << "Error" << std::endl;
   break;

  case( SDDPSolver::kUnbounded ):
   std::cout << "A subproblem is unbounded." << std::endl;
   break;

  case( SDDPSolver::kInfeasible ):
   std::cout << "A subproblem is infeasible." << std::endl;
   break;

  case( SDDPSolver::kStopTime ):
   std::cout << "The solution process terminated due a time limit."
             << std::endl;
   break;

  case( SDDPSolver::kStopIter ):
   std::cout << "The solution process terminated due an iteration limit."
             << std::endl;
   break;
 }
}

/*--------------------------------------------------------------------------*/

void solve( SDDPBlock * sddp_block ) {

 auto solver = dynamic_cast< SDDPSolver * >
  ( sddp_block->get_registered_solvers().front() );

 if( ! solver )
  throw( std::logic_error( "The Solver for the SDDPBlock must be a "
                           "SDDPSolver." ) );

 solver->set_log( &std::cout );

 auto status = solver->compute();

 show_status( status );

 SDDPBlockSolutionOutput o;
 o.print_cuts( sddp_block , "BellmanValuesAllOUT.csv" );

 CutProcessing().remove_redundant_cuts
  ( static_cast< SDDPBlock * >( sddp_block ) );
 o.print_cuts( sddp_block , "BellmanValuesOUT.csv" );
}

/*--------------------------------------------------------------------------*/

void configure_Blocks( SDDPBlock * sddp_block , bool relax_binary_variables ) {
 for( auto sub_block : sddp_block->get_nested_Blocks() ) {

  auto stochastic_block = static_cast<StochasticBlock *>( sub_block );
  auto benders_block = static_cast<BendersBlock *>
   ( stochastic_block-> get_nested_Blocks().front() );
  auto objective = static_cast<FRealObjective *>
   ( benders_block->get_objective() );
  auto benders_function = static_cast<BendersBFunction *>
   ( objective->get_function() );
  auto inner_block = benders_function->get_inner_block();

  std::queue< Block *> blocks;
  blocks.push( inner_block );

  while( ! blocks.empty() ) {
   auto block = blocks.front();
   blocks.pop();
   auto n = block->get_number_nested_Blocks();
   for( decltype( n ) i = 0 ; i < n ; ++i ) {
    blocks.push( block->get_nested_Block( i ) );
   }

   int var_type = 0;
   if( relax_binary_variables ) var_type = 1;
   int cons_type = 1; // generate OneVarConstraints

   // Configure PolyhedralFunctionBlock
   if( auto polyhedral = dynamic_cast< PolyhedralFunctionBlock * >( block ) ) {
    auto config = new BlockConfig;
    config->f_static_variables_Configuration = new SimpleConfiguration<int>(1);
    polyhedral->set_BlockConfig( config );
   }

   else if( auto unit = dynamic_cast< SlackUnitBlock * >( block ) ) {
    auto config = new BlockConfig;
    config->f_static_variables_Configuration =
     new SimpleConfiguration<int>( var_type );
    config->f_static_constraints_Configuration =
     new SimpleConfiguration<int>( cons_type );
    unit->set_BlockConfig( config );
   }

   else if( auto unit = dynamic_cast< BatteryUnitBlock * >( block ) ) {
    auto config = new BlockConfig;
    config->f_static_variables_Configuration =
     new SimpleConfiguration<int>( var_type );
    config->f_static_constraints_Configuration =
     new SimpleConfiguration<int>( cons_type );
    unit->set_BlockConfig( config );
   }

   else if( auto unit = dynamic_cast< ThermalUnitBlock * >( block ) ) {
    auto config = new BlockConfig;
    config->f_static_variables_Configuration =
     new SimpleConfiguration<int>( var_type );
    config->f_static_constraints_Configuration =
     new SimpleConfiguration<int>( cons_type );
    unit->set_BlockConfig( config );
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
  auto sddp_block = dynamic_cast<SDDPBlock *>( Block::new_Block( block_group ) );
  if( ! sddp_block )
   throw( std::logic_error( "Error while deserializing the SDDPBlock." ) );

  // Configure block
  auto block_config_group = problem_group.getGroup( "BlockConfig" );
  auto block_config = static_cast<BlockConfig *>
   ( BlockConfig::new_Configuration( block_config_group ) );
  if( ! block_config )
   throw( std::logic_error("BlockConfig group was not properly provided.") );
  block_config->apply( sddp_block );
  block_config->clear();

  // Configure solver
  auto solver_config_group = problem_group.getGroup( "BlockSolver" );
  auto block_solver_config = static_cast<BlockSolverConfig *>
   ( BlockSolverConfig::new_Configuration( solver_config_group ) );
  if( ! block_solver_config )
   throw( std::logic_error("BlockSolver group was not properly provided.") );
  block_solver_config->apply( sddp_block );
  block_solver_config->clear();

  std::cout << "Problem: " << problem.first << std::endl;

  // Solve

  if( simulation_mode )
   simulate( sddp_block );
  else
   solve( sddp_block );

  // Destroy the Block and the Configurations

  block_config->apply( sddp_block );
  delete block_config;

  block_solver_config->apply( sddp_block );
  delete block_solver_config;

  delete sddp_block;
 }
}

/*--------------------------------------------------------------------------*/

BlockSolverConfig * build_BlockSolverConfig() {
 auto block_solver_config = new BlockSolverConfig;
 if( simulation_mode ) {
  auto config = new ComputeConfig;
  config->set_par( "intLogVerb" , 1 );
  block_solver_config->add_ComputeConfig( "SDDPGreedySolver" , config );
 }
 else {
  auto config = new ComputeConfig;
  config->set_par( "intLogVerb" , 100 );
  //config->set_par( "dblAccuracy" , 1.0e-3 );
  //config->set_par( "intNbSimulBackward" , 100 );
  //config->set_par( "intNbSimulForward" , 5 );
  //config->set_par( "intNStepConv" , 5 );

  block_solver_config->add_ComputeConfig( "SDDPSolver" , config );
 }

 return block_solver_config;
}

/*--------------------------------------------------------------------------*/

BlockConfig * build_BlockConfig( const SDDPBlock * sddp_block ) {
 // TODO configure all PolyhedralFunctionBlock
 auto sddp_config = new RBlockConfig;
 auto num_stochastic_blocks = sddp_block->get_number_nested_Blocks();

 for( Block::Index index = 0 ; index < num_stochastic_blocks ; ++index ) {

  auto cpx_compute_config = new ComputeConfig;
  if( continuous_relaxation ) {
   cpx_compute_config->set_par( "CPXPARAM_Preprocessing_Presolve" , 0 );
   cpx_compute_config->set_par( "intThrowReducedCostException" , 1 );
  }

  auto inner_benders_function_solver = new BlockSolverConfig;
  inner_benders_function_solver->add_ComputeConfig( "CPXMILPSolver" ,
                                                    cpx_compute_config );

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

 return sddp_config;
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
  block_config = dynamic_cast<BlockConfig *>
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
 return block_config;
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
  solver_config = dynamic_cast<BlockSolverConfig *>
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
 return solver_config;
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
 bool block_solver_config_provided = true;
 auto solver_config = load_BlockSolverConfig();
 if( ! solver_config ) {
  block_solver_config_provided = false;
  solver_config = build_BlockSolverConfig();
 }

 auto cleared_solver_config = solver_config->clone();
 cleared_solver_config->clear();

 // For each Block descriptor
 for( auto block_description : blocks ) {

  // Deserialize the SDDPBlock

  auto sddp_block = dynamic_cast<SDDPBlock *>
   ( Block::new_Block( block_description.second ) );

  if( ! sddp_block )
   throw( std::logic_error( "Error while deserializing the SDDPBlock." ) );

  // Configure the SDDPBlock

  if( given_block_config )
   given_block_config->apply( sddp_block );
  else {
   configure_Blocks( sddp_block , ( ! simulation_mode ) || relax_integrality );

   if( ! block_solver_config_provided ) {
    block_config = build_BlockConfig( sddp_block );
    block_config->apply( sddp_block );
    block_config->clear();
   }
  }

  // Configure the Solver

  solver_config->apply( sddp_block );

  // Solve

  if( simulation_mode ) {
   auto solver = sddp_block->get_registered_solvers().front();
   if( solver->get_int_par( solver->int_par_str2idx( "intLogVerb" ) ) )
    solver->set_log( & std::cout );
   simulate( sddp_block );
  }
  else
   solve( sddp_block );

  // Destroy the SDDPBlock and the Configurations

  if( block_config )
   block_config->apply( sddp_block );
  if( ! given_block_config ) {
   delete block_config;
   block_config = nullptr;
  }

  cleared_solver_config->apply( sddp_block );
  delete sddp_block;
 }

 delete block_config;
 delete given_block_config;
 delete solver_config;
 delete cleared_solver_config;
}

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv ) {

 docopt_desc = "SMS++ SDDP solver.\n";
 exe = get_filename( argv[ 0 ] );
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

 file.close();
 return 0;
}
