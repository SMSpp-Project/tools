/*--------------------------------------------------------------------------*/
/*-------------------------- File sddp_solver.cpp --------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * This is a convenient tool for solving an SDDPBlock using either the
 * SDDPSolver or the SDDPGreedySolver. The description of the SDDPBlock must
 * be given in a netCDF file. This tool can be executed as follows:
 *
 *   ./sddp_solver [-s] [-e] [-l FILE] [-i INDEX] [-m NUMBER] [-t STAGE]
 *                 [-n NUMBER] [-B FILE] [-S FILE] [-p PATH] [-c PATH]
 *                 <nc4-file>
 *
 * The only mandatory argument is the netCDF file containing the description
 * of the SDDPBlock. This netCDF file can be either a BlockFile or a
 * ProbFile. The BlockFile can contain any number of child groups, each one
 * describing an SDDPBlock. Every SDDPBlock is then solved. The ProbFile can
 * also contain any number of child groups, each one having the description of
 * an SDDPBlock alongside the description of a BlockConfig and a
 * BlockSolverConfig for the SDDPBlock. Also in this case, every SDDPBlock is
 * solved.
 *
 * The -c option specifies the prefix to the paths to all configuration
 * files. This means that if PATH is the value passed to the -c option, then
 * the name (or path) to each configuration file will be prepended by
 * PATH. The -p option specifies the prefix to the paths to all files
 * specified by the attribute "filename" in the input netCDF file.
 *
 * The -s option indicates whether a simulation must be performed. If this
 * option is used, then the SDDPBlock is solved using the
 * SDDPGreedySolver. Otherwise, the SDDPBlock is solved by the SDDPSolver.
 *
 * In simulation mode (i.e., when the -s option is used), the -i option
 * specifies the index of the scenario for which the problem must be
 * solved. The index must be a number between 0 and n-1, where n is the number
 * of scenarios in the SDDPBlock. If this index is not provided, then the
 * problem is solved for the first scenario. Also in simulation mode, the -m
 * option indicates that consecutive simulations must be
 * performed. Consecutive simulations are simulations which are launched in
 * sequence, one after the other, and which are linked by the storage
 * levels. The final state of some stage of a simulation is used as the
 * initial state for the next simulation. See the comments below for more
 * details. If the value NUMBER provided by this option is greater than 1,
 * then NUMBER consecutive simulations are performed.
 *
 * The -n option specifies the number of sub-Blocks of SDDPBlock that must be
 * constructed for each stage. By default, SDDPBlock contains a single
 * sub-Blocks for each stage. This option must be provided in order to solve
 * multiple scenarios in parallel. In this case, the number of scenarios that
 * are solved in parallel is n (assuming n is not larger than the number of
 * scenarios).
 *
 * The -B and -S options are only considered if the given netCDF file is a
 * BlockFile. The -B option specifies a BlockConfig file to be applied to
 * every SDDPBlock; while the -S option specifies a BlockSolverConfig file for
 * every SDDPBlock. If each of these options is not provided when the given
 * netCDF file is a BlockFile, then default configurations are considered.
 *
 * Initial cuts can be provided by using the -l option. This option must be
 * followed by the path to the file containing the initial cuts. This file
 * must have the following format. The first line contains a header and its
 * content is ignored. Each of the following lines represent a cut and has the
 * following format:
 *
 *     t, a_0, a_1, ..., a_k, b
 *
 * where t is a stage (an integer between 0 and time horizon minus 1), a_0,
 * ..., a_k are the coefficients of the cut, and b is the constant term of the
 * cut.
 *
 * As a preprocessing, given redundant cuts can be removed by using the -e
 * option. Notice that all cuts will be subject to being removed, whether they
 * are provided in a netCDF file or by the -l option.
 *
 * There are a few ways to specify the initial state for the first stage
 * subproblem. This can be done by setting the initial state variable of
 * SDDPBlock or by setting the initial state parameter of SDDPSolver or
 * SDDPGreedySolver. When running multiple simulations (when both the -s and
 * -m options are used), there is an additional way to specify the initial
 * state. The (final) state of some stage from a simulation can be used as the
 * initial state for the first stage of the next simulation. The stage at
 * which the state can be taken to serve as the initial state for the next
 * simulation can be specified by the -t option. This option must be followed
 * by an integer number STAGE. If STAGE is between 0 and T-1, where T is the
 * time horizon of the problem, then the solution (final state) of the
 * subproblem associated with stage STAGE of a simulation will serve as the
 * initial state for the first stage subproblem of the next simulation. If
 * STAGE does not belong to that interval (that is, if it is negative or
 * greater than or equal to T) or if the -t option is not used, then no
 * changes are made to the way the initial state is specified.
 *
 * \author Rafael Durbano Lobato \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Rafael Durbano Lobato
 */

#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <queue>

#include <BendersBlock.h>
#include <BlockSolverConfig.h>
#include <HydroSystemUnitBlock.h>
#include <RBlockConfig.h>
#include <SDDPBlock.h>
#include <StochasticBlock.h>
#include <SDDPGreedySolver.h>
#include <SDDPSolver.h>

#include "CutProcessing.h"
#include "SDDPBlockSolutionOutput.h"

#ifdef USE_MPI
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#endif

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/

std::string filename{};
std::string block_config_filename{};
std::string solver_config_filename{};
std::string config_filename_prefix{};
std::string cuts_filename{};
long scenario_id = 0;
long num_sub_blocks_per_stage = 1;
long number_simulations = 1;
long initial_solution_stage = -1;
bool simulation_mode = false;
bool eliminate_redundant_cuts = false;
const bool force_hard_components = false;
const bool continuous_relaxation = true;

// This variable indicates whether negative prices may occur
const bool negative_prices = false;

std::string exe{};         ///< Name of the executable file
std::string docopt_desc{}; ///< Tool description

// It replaces any zero value that the IntermittentUnitBlock maximum power may
// assume
const double epsilon_max_power = 1.0e-16;

/// Tolerance to be considered in Block::is_feasible()
const double feasibility_tolerance = 1.0e-6;

/// Type of violation to be considered in Block::is_feasible()
const bool relative_violation = true;

// Name of Configuration files for each component of the Lagrangian dual of
// the UCBlock
const std::string thermal_config_filename = "TUBSCfg.txt";
const std::string hydro_config_filename = "HSUBSCfg.txt";
const std::string other_unit_config_filename = "OUBSCfg.txt";
const std::string default_config_filename = "LPBSCfg.txt";

/*--------------------------------------------------------------------------*/

// Gets the name of the executable from its full path
std::string get_filename( const std::string & fullpath ) {
 std::size_t found = fullpath.find_last_of( "/\\" );
 return( fullpath.substr( found + 1 ) );
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
           << "  -B, --blockcfg <file>           Block configuration.\n"
           << "  -c, --configdir <path>          The prefix for all config filenames.\n"
           << "  -e, --eliminate-redundant-cuts  Eliminate given redundant cuts.\n"
           << "  -h, --help                      Print this help.\n"
           << "  -i, --scenario <index>          The index of the scenario.\n"
           << "  -l, --load-cuts <file>          Load cuts from a file.\n"
           << "  -m, --num-simulations <number>  Number of simulations to be performed.\n"
           << "  -n, --num-blocks <number>       Number of sub-Blocks per stage.\n"
           << "  -p, --prefix <path>             The prefix for all Block filenames.\n"
           << "  -s, --simulation                Simulation mode.\n"
           << "  -S, --solvercfg <file>          Solver configuration.\n"
           << "  -t, --stage <stage>             Stage from which initial state is taken."
           << std::endl;
}

/*--------------------------------------------------------------------------*/

long get_long_option() {
 char * end = nullptr;
 errno = 0;
 long option = std::strtol( optarg , &end , 10 );
 if( ( ! optarg ) || ( ( option = std::strtol( optarg , &end , 10 ) ) ,
                       ( errno || ( end && *end ) ) ) ) {
  option = -1;
 }
 return( option );
}

/*--------------------------------------------------------------------------*/

void process_args( int argc , char ** argv ) {

 if( argc < 2 ) {
  std::cout << exe << ": no input file\n"
            << "Try " << exe << "' --help' for more information.\n";
  exit( 1 );
 }

 const char * const short_opts = "B:c:hei:l:m:n:p:rsS:t:";
 const option long_opts[] = {
  { "blockcfg" ,                 required_argument , nullptr , 'B' } ,
  { "configdir" ,                required_argument , nullptr , 'c' } ,
  { "help" ,                     no_argument ,       nullptr , 'h' } ,
  { "eliminate-redundant-cuts" , no_argument ,       nullptr , 'e' } ,
  { "scenario" ,                 required_argument , nullptr , 'i' } ,
  { "load-cuts" ,                required_argument , nullptr , 'l' } ,
  { "num-simulations" ,          required_argument , nullptr , 'm' } ,
  { "num-blocks" ,               required_argument , nullptr , 'n' } ,
  { "prefix" ,                   required_argument , nullptr , 'p' } ,
  { "relax" ,                    no_argument ,       nullptr , 'r' } ,
  { "simulation" ,               no_argument ,       nullptr , 's' } ,
  { "solvercfg" ,                required_argument , nullptr , 'S' } ,
  { "stage" ,                    required_argument , nullptr , 't' } ,
  { nullptr ,                    no_argument ,       nullptr , 0 }
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
    config_filename_prefix = std::string( optarg );
    Configuration::set_filename_prefix( std::string( optarg ) );
    break;
   case 'e':
    eliminate_redundant_cuts = true;
    break;
   case 'i': {
    scenario_id = get_long_option();
    if( scenario_id < 0 ) {
     std::cout << "The index of the scenario must be a nonnegative integer."
               << std::endl;
     exit( 1 );
    }
    break;
   }
   case 'l':
    cuts_filename = std::string( optarg );
    break;
   case 'm': {
    number_simulations = get_long_option();
    if( number_simulations < 1 ) {
     std::cout << "The number of simulations must be at least 1." << std::endl;
     exit( 1 );
    }
    break;
   }
   case 'n': {
    num_sub_blocks_per_stage = get_long_option();
    if( num_sub_blocks_per_stage <= 0 ) {
     std::cout << "The number of sub-Blocks per stage must be a "
               << "positive integer." << std::endl;
     exit( 1 );
    }
    break;
   }
   case 'p':
    Block::set_filename_prefix( std::string( optarg ) );
    break;
   case 'r':
    std::cout << "The -r option no longer exists. In order relax the "
              << "integrality constraints,\nplease properly configure the "
              << "solver. For instance, some solvers have the\nparameter "
              << "'intRelaxIntVars', which can be set to 1 in the solver\n"
              << "configuration file associated with the Block whose "
              << "constraints must be\nrelaxed." << std::endl;
    exit( 1 );
   case 's':
    simulation_mode = true;
    break;
   case 'S':
    solver_config_filename = std::string( optarg );
    break;
   case 't':
    initial_solution_stage = get_long_option();
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
             << " terminated due to an iteration limit." << std::endl;
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

 return( benders_function->get_inner_block() );
}

/*--------------------------------------------------------------------------*/

bool update_hydro_unit( Block * previous_block , Block * block ,
                        const Index stage ) {
 auto unit = dynamic_cast< HydroUnitBlock * >( block );
 auto previous_unit = dynamic_cast< HydroUnitBlock * >( previous_block );

 if( ( ! unit ) && ( ! previous_unit ) )
  return( false );

 if( ( ! unit ) || ( ! previous_unit ) )
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

 const auto time_horizon = previous_unit->get_time_horizon();

 std::vector< double > flow_rate( number_generators );

 for( Index g = 0 ; g < number_generators ; ++g )
  flow_rate[ g ] =
   previous_unit->get_flow_rate( g , time_horizon - 1 )->get_value();

 unit->set_initial_flow_rate( flow_rate.cbegin() );

 return( true );
}

/*--------------------------------------------------------------------------*/

bool update_battery_unit( Block * previous_block , Block * block ,
                          const Index stage ) {
 auto unit = dynamic_cast< BatteryUnitBlock * >( block );
 auto previous_unit = dynamic_cast< BatteryUnitBlock * >( previous_block );

 if( ( ! unit ) && ( ! previous_unit ) )
  return( false );

 if( ( ! unit ) || ( ! previous_unit ) )
  throw( std::logic_error
         ( "sddp_solver: UCBlocks at stages " + std::to_string( stage - 1 ) +
           " and " + std::to_string( stage ) +
           " do not have the same structure." ) );

 const auto time_horizon = previous_unit->get_time_horizon();

 std::vector< double > initial_power_data =
  { ( previous_unit->get_active_power( 0 ) + time_horizon - 1 )->get_value() };

 unit->set_initial_power( initial_power_data.cbegin() );

 std::vector< double > initial_storage_data =
  { previous_unit->get_storage_level()[ time_horizon - 1 ].get_value() };

 unit->set_initial_storage( initial_storage_data.cbegin() );

 return( true );
}

/*--------------------------------------------------------------------------*/

int compute_init_up_down_time( const SDDPBlock * sddp_block ,
                               ThermalUnitBlock * previous_unit ,
                               ThermalUnitBlock * unit , const Index stage ) {

 auto time_horizon = previous_unit->get_time_horizon();
 auto commitment = previous_unit->get_commitment( 0 ) + time_horizon - 1;

 auto shutdown = previous_unit->get_shut_down( time_horizon - 1 );
 if( shutdown && shutdown->get_value() >= 0.5 ) {
  return( 0 );
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
    return( init_up_down_time );
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

  time_horizon = previous_unit->get_time_horizon();

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

 return( init_up_down_time );
}

/*--------------------------------------------------------------------------*/

bool update_thermal_unit( const SDDPBlock * sddp_block ,
                          Block * previous_block , Block * block ,
                          const Index stage ) {

 auto previous_unit = dynamic_cast< ThermalUnitBlock * >( previous_block );
 auto unit = dynamic_cast< ThermalUnitBlock * >( block );

 if( ( ! unit ) && ( ! previous_unit ) )
  return( false );

 if( ( ! unit ) || ( ! previous_unit ) )
  throw( std::logic_error
         ( "sddp_solver: UCBlocks at stages " + std::to_string( stage - 1 ) +
           " and " + std::to_string( stage ) +
           " do not have the same structure." ) );

 if( simulation_mode ) {
  auto init_up_down_time = compute_init_up_down_time
   ( sddp_block , previous_unit , unit , stage );

  std::vector< int > init_up_down_time_data = { init_up_down_time };
  unit->set_init_updown_time( init_up_down_time_data.cbegin() );
 }

 const auto time_horizon = previous_unit->get_time_horizon();

 std::vector< double > active_power_data =
  { ( previous_unit->get_active_power( 0 ) + time_horizon - 1 )->get_value() };
 unit->set_initial_power( active_power_data.cbegin() );

 return( true );
}

/*--------------------------------------------------------------------------*/

void callback( SDDPBlock * sddp_block , Block::Index stage ) {

 if( stage == 0 )
  return;

 auto previous_uc_block = get_uc_block( sddp_block , stage - 1 );
 auto uc_block = get_uc_block( sddp_block , stage );

 std::queue< Block * > blocks;
 blocks.push( uc_block );

 std::queue< Block * > previous_blocks;
 previous_blocks.push( previous_uc_block );

 while( ! blocks.empty() ) {
  auto block = blocks.front();
  blocks.pop();

  auto previous_block = previous_blocks.front();
  previous_blocks.pop();

  auto n = block->get_number_nested_Blocks();

  if( n != previous_block->get_number_nested_Blocks() ) {
   throw( std::logic_error
          ( "sddp_solver: UCBlocks at stages " + std::to_string( stage - 1 ) +
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
                           "SDDPGreedySolver in simulation mode." ) );

 solver->set_callback( [ sddp_block ]( Index stage ) {
  callback( sddp_block , stage );
 });

 // Load possibly given cuts

 if( ! cuts_filename.empty() )
  solver->set_par( SDDPGreedySolver::strLoadCuts , cuts_filename );

 // Eliminate redundant cuts if it is desired

 if( eliminate_redundant_cuts )
  CutProcessing().remove_redundant_cuts( sddp_block );

 solver->set_scenario_id( scenario_id );

 auto status = solver->compute();

#ifdef USE_MPI
 boost::mpi::communicator world;
 if( world.rank() == 0 ) {
#endif

 show_simulation_status( status , solver->get_fault_stage() );

 SDDPBlockSolutionOutput output;

 if( solver->has_var_solution() ) {
  solver->get_var_solution();
  output.print( sddp_block );
 }
 else
  output.print( sddp_block , solver->get_fault_stage() );

 auto lb = solver->get_lb();
 auto ub = solver->get_ub();

 std::cout << "Lower bound: " << std::setprecision( 20 ) << lb << std::endl;
 std::cout << "Upper bound: " << std::setprecision( 20 ) << ub << std::endl;

#ifdef USE_MPI
 }
#endif
}

/*--------------------------------------------------------------------------*/

void show_status( Index status ) {

 switch( status ) {

  case( SDDPSolver::kOK ):
   std::cout << "Optimal solution found." << std::endl;
   break;

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
   std::cout << "The solution process terminated due to an iteration limit."
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
                           "SDDPSolver in optimization mode." ) );

 solver->set_log( &std::cout );

 auto status = solver->compute();

 show_status( status );

 SDDPBlockSolutionOutput o;
 o.print_cuts( sddp_block , "BellmanValuesAllOUT.csv" );

 if( eliminate_redundant_cuts )
  CutProcessing().remove_redundant_cuts
   ( static_cast< SDDPBlock * >( sddp_block ) );

 o.print_cuts( sddp_block , "BellmanValuesOUT.csv" );
}

/*--------------------------------------------------------------------------*/

void load_cuts( SDDPBlock * sddp_block ) {
 if( cuts_filename.empty() )
  return;

 std::ifstream cuts_file( cuts_filename );

 // Make sure the file is open
 if( ! cuts_file.is_open() )
  throw( std::runtime_error( "It was not possible to open the file \"" +
                             cuts_filename + "\"." ) );

 const auto time_horizon = sddp_block->get_time_horizon();

 std::vector< PolyhedralFunction::MultiVector > A
  ( time_horizon , PolyhedralFunction::MultiVector{} );
 std::vector< PolyhedralFunction::RealVector > b
  ( time_horizon , PolyhedralFunction::RealVector{} );

 std::string line;

 if( cuts_file.good() )
  // Skip the first line containing the header
  std::getline( cuts_file , line );

 int line_number = 0;

 // Read the cuts

 while( std::getline( cuts_file , line ) ) {
  ++line_number;

  std::stringstream line_stream( line );

  // Try to read the stage
  Index stage;
  if( ! ( line_stream >> stage ) )
   break;

  if( stage >= time_horizon )
   throw( std::logic_error( "File \"" + cuts_filename + "\" contains an invalid"
                            " stage: " + std::to_string( stage ) + "." ) );

  if( line_stream.peek() != ',' )
   throw( std::logic_error( "File \"" + cuts_filename +
                            "\" has an invalid format." ) );
  line_stream.ignore();

  // Read the cut

  const auto polyhedral_function = sddp_block->get_polyhedral_function( stage );
  const auto num_active_var = polyhedral_function->get_num_active_var();
  PolyhedralFunction::RealVector a( num_active_var );

  Index i = 0;
  double value;
  while( line_stream >> value ) {
   if( i > num_active_var )
    throw( std::logic_error
           ( "File \"" + cuts_filename + "\" contains an invalid"
             " cut at line " + std::to_string( line_number ) + "." ) );

   if( i < num_active_var )
    a[ i ] = value;
   else
    b[ stage ].push_back( value );

   ++i;

   if( line_stream.peek() == ',' )
    line_stream.ignore();
  }

  if( i < num_active_var )
   throw( std::logic_error
          ( "File \"" + cuts_filename + "\" contains an invalid"
            " cut at line " + std::to_string( line_number ) + "." ) );

  A[ stage ].push_back( a );
 }

 cuts_file.close();

 // Now, add the cuts to all PolyhedralFunctions

 const auto num_sub_blocks_per_stage =
  sddp_block->get_num_sub_blocks_per_stage();

 for( Index stage = 0 ; stage < time_horizon ; ++stage ) {
  for( Index sub_block_index = 0 ; sub_block_index < num_sub_blocks_per_stage ;
       ++sub_block_index ) {

   if( b[ stage ].empty() )
    continue; // no cut for this stage

   // We assume that there is only one PolyhedralFunction per stage
   auto polyhedral_function =
    sddp_block->get_polyhedral_function( stage , 0 , sub_block_index );

   // Copy the A matrix for this stage so that it can be moved
   auto A_stage = A[ stage ];

   polyhedral_function->add_rows( std::move( A_stage ) , b[ stage ] );
  }
 }
}

/*--------------------------------------------------------------------------*/

bool using_thermal_dp_solver( const std::string & config_filename ) {
 std::ifstream stream( config_filename );

 if( ! stream.is_open() ) {
  std::cerr << "Solver configuration " + config_filename +
   " was not found." << std::endl;
  exit( 1 );
 }

 std::string config_name;
 stream >> eatcomments >> config_name;
 auto config = Configuration::new_Configuration( config_name );
 auto solver_config = dynamic_cast< BlockSolverConfig * >( config );

 if( ! solver_config ) {
  std::cerr << "Solver configuration is not valid: "
            << config_name << std::endl;
  delete( config );
  exit( 1 );
 }

 try {
  stream >> *solver_config;
 }
 catch( ... ) {
  std::cout << "Error while loading Solver configuration: "
            << config_name << std::endl;
  delete( config );
  exit( 1 );
 }

 for( const auto & solver_name : solver_config->get_SolverNames() )
  if( solver_name == "ThermalUnitDPSolver" ) {
   delete( config );
   return( true );
  }

 delete( config );
 return( false );
}

/*--------------------------------------------------------------------------*/

void configure_Blocks( SDDPBlock * sddp_block ,
                       bool add_reserve_variables_to_objective ,
                       double feasibility_tolerance , bool relative_violation ,
                       bool is_using_lagrangian_dual_solver ) {

 const SimpleConfiguration< std::pair< double , int > >
  is_feasible_config( { feasibility_tolerance , relative_violation } );

 const int cons_type = 1; // generate OneVarConstraints

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

   // Configure PolyhedralFunctionBlock
   if( auto polyhedral = dynamic_cast< PolyhedralFunctionBlock * >( block ) ) {
    auto config = new BlockConfig;
    config->f_static_variables_Configuration = new SimpleConfiguration< int >( 1 );
    config->f_is_feasible_Configuration = is_feasible_config.clone();
    polyhedral->set_BlockConfig( config );
   }

   else if( auto unit = dynamic_cast< SlackUnitBlock * >( block ) ) {
    auto config = new BlockConfig;
    config->f_static_constraints_Configuration =
     new SimpleConfiguration< int >( cons_type );
    config->f_is_feasible_Configuration = is_feasible_config.clone();
    unit->set_BlockConfig( config );
   }

   else if( auto unit = dynamic_cast< BatteryUnitBlock * >( block ) ) {
    auto config = new BlockConfig;
    config->f_static_variables_Configuration =
      new SimpleConfiguration< int >( negative_prices );
    config->f_static_constraints_Configuration =
     new SimpleConfiguration< int >( cons_type );
    config->f_is_feasible_Configuration = is_feasible_config.clone();
    unit->set_BlockConfig( config );
   }

   else if( auto unit = dynamic_cast< ThermalUnitBlock * >( block ) ) {
    auto config = new BlockConfig;
    config->f_static_constraints_Configuration =
     new SimpleConfiguration< int >( cons_type );

    if( add_reserve_variables_to_objective )
     config->f_objective_Configuration = new SimpleConfiguration< int >( 3 );

    config->f_is_feasible_Configuration = is_feasible_config.clone();

    unit->set_BlockConfig( config );
   }
   else if( auto unit = dynamic_cast< IntermittentUnitBlock * >( block ) ;
            unit && is_using_lagrangian_dual_solver ) {
    auto config = new BlockConfig;
    config->f_is_feasible_Configuration = is_feasible_config.clone();
    config->f_extra_Configuration =
     new SimpleConfiguration< double >( epsilon_max_power );
    unit->set_BlockConfig( config );
   }

   else {
    auto config = new BlockConfig;
    config->f_is_feasible_Configuration = is_feasible_config.clone();
    block->set_BlockConfig( config );
   }

  }
 }
}

/*--------------------------------------------------------------------------*/

void set_log( SDDPBlock * sddp_block , std::ostream * output_stream ) {
 for( auto sub_block : sddp_block->get_nested_Blocks() ) {
  auto stochastic_block = static_cast< StochasticBlock * >( sub_block );
  auto benders_block = static_cast< BendersBlock * >
   ( stochastic_block-> get_nested_Blocks().front() );
  auto objective = static_cast< FRealObjective * >
   ( benders_block->get_objective() );
  auto benders_function = static_cast< BendersBFunction * >
   ( objective->get_function() );
  auto inner_block = benders_function->get_inner_block();
  for( auto solver : inner_block->get_registered_solvers() )
   if( solver )
    solver->set_log( output_stream );
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
  auto sddp_block = new SDDPBlock;
  sddp_block->set_num_sub_blocks_per_stage( num_sub_blocks_per_stage );
  sddp_block->deserialize( block_group );

  // Configure block
  auto block_config_group = problem_group.getGroup( "BlockConfig" );
  auto block_config = static_cast< BlockConfig * >
   ( BlockConfig::new_Configuration( block_config_group ) );
  if( ! block_config )
   throw( std::logic_error( "BlockConfig group was not properly provided." ) );
  block_config->apply( sddp_block );
  block_config->clear();

  // Configure solver
  auto solver_config_group = problem_group.getGroup( "BlockSolver" );
  auto block_solver_config = static_cast< BlockSolverConfig * >
   ( BlockSolverConfig::new_Configuration( solver_config_group ) );
  if( ! block_solver_config )
   throw( std::logic_error( "BlockSolver group was not properly provided." ) );
  block_solver_config->apply( sddp_block );
  block_solver_config->clear();

  // Set the output stream for the log of the inner Solvers

  set_log( sddp_block , &std::cout );

  // Load possibly given cuts

  if( ! simulation_mode )
   load_cuts( sddp_block );

  // Eliminate redundant cuts if it is desired

  if( eliminate_redundant_cuts )
   CutProcessing().remove_redundant_cuts( sddp_block );


  std::cout << "Problem: " << problem.first << std::endl;

  // Solve

  if( simulation_mode )
   simulate( sddp_block );
  else
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
 if( simulation_mode ) {
  auto config = new ComputeConfig;
  config->set_par( "intLogVerb" , 1 );
  block_solver_config->add_ComputeConfig( "SDDPGreedySolver" , config );
 }
 else {
  auto config = new ComputeConfig;
  config->set_par( "intLogVerb" , 1 );
  block_solver_config->add_ComputeConfig( "SDDPSolver" , config );
 }

 return( block_solver_config );
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
   new SimpleConfiguration< std::map< std::string , Configuration * > >
   ( { { "BlockConfig" , nullptr } ,
       { "BlockSolverConfig" , inner_benders_function_solver } } );

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

 if( block_config_filename.empty() ) {
  std::cout << "Block configuration was not provided. "
   "Using default configuration." << std::endl;
  return( nullptr );
 }

 std::ifstream block_config_file;
 block_config_file.open( block_config_filename , std::ifstream::in );

 if( ! block_config_file.is_open() ) {
  std::cerr << "Block configuration " + block_config_filename +
   " was not found." << std::endl;
  exit( 1 );
 }

 std::cout << "Using Block configuration in " << block_config_filename
           << "." << std::endl;

 std::string config_name;
 block_config_file >> eatcomments >> config_name;
 auto config = Configuration::new_Configuration( config_name );
 auto block_config = dynamic_cast< BlockConfig * >( config );

 if( ! block_config ) {
  std::cerr << "Block configuration is not valid: "
            << config_name << std::endl;
  delete( config );
  exit( 1 );
 }

 try {
  block_config_file >> *block_config;
 }
 catch( const std::exception& e ) {
  std::cerr << "Block configuration is not valid: " << e.what() << std::endl;
  exit( 1 );
 }

 block_config_file.close();
 return( block_config );
}

/*--------------------------------------------------------------------------*/

BlockSolverConfig * load_BlockSolverConfig( const std::string & filename ) {

 if( filename.empty() ) {
  std::cout << "Solver configuration was not provided. "
   "Using default configuration." << std::endl;
  return( nullptr );
 }

 std::ifstream solver_config_file;
 solver_config_file.open( filename , std::ifstream::in );

 if( ! solver_config_file.is_open() ) {
  std::cerr << "Solver configuration " + filename +
   " was not found." << std::endl;
  exit( 1 );
 }

 std::cout << "Using Solver configuration in " << filename << "." << std::endl;

 std::string config_name;
 solver_config_file >> eatcomments >> config_name;
 auto config = Configuration::new_Configuration( config_name );
 auto solver_config = dynamic_cast< BlockSolverConfig * >( config );

 if( ! solver_config ) {
  std::cerr << "Solver configuration is not valid: "
            << config_name << std::endl;
  delete( config );
  exit( 1 );
 }

 try {
  solver_config_file >> *solver_config;
 }
 catch( ... ) {
  std::cout << "Solver configuration is not valid." << std::endl;
  exit( 1 );
 }

 solver_config_file.close();
 return( solver_config );
}

/*--------------------------------------------------------------------------*/

std::string get_str_par( ComputeConfig * compute_config ,
                         std::string par_name ) {
 for( const auto & pair : compute_config->str_pars ) {
  if( pair.first == par_name )
   return( pair.second );
 }
 return( "" );
}

/*--------------------------------------------------------------------------*/

int get_int_par( ComputeConfig * compute_config , std::string par_name ) {
 for( const auto & pair : compute_config->int_pars ) {
  if( pair.first == par_name )
   return( pair.second );
 }
 return( Inf< int >() );
}

/*--------------------------------------------------------------------------*/

bool using_lagrangian_dual_solver( BlockSolverConfig * sddp_solver_config ) {

 if( ! sddp_solver_config )
  return( false );

 BlockSolverConfig * inner_solver_config = nullptr;
 ComputeConfig * compute_config = nullptr;

 for( Index i = 0 ; i < sddp_solver_config->num_ComputeConfig() ; ++i ) {

  if( sddp_solver_config->get_SolverName( i ) != "SDDPSolver" &&
      sddp_solver_config->get_SolverName( i ) != "ParallelSDDPSolver" &&
      sddp_solver_config->get_SolverName( i ) != "SDDPGreedySolver" )
   continue;

  compute_config = sddp_solver_config->get_SolverConfig( i );

  // Check if strInnerBSC is present

  auto strInnerBSC = get_str_par( compute_config , "strInnerBSC" );

  if( strInnerBSC.empty() )
   continue;

  // If it is, check if it is a config for a LagrangianDualSolver

  std::ifstream inner_solver_config_file
   ( config_filename_prefix + strInnerBSC , std::ifstream::in );

  if( ! inner_solver_config_file.is_open() )
   continue;

  std::string inner_config_name;
  inner_solver_config_file >> eatcomments >> inner_config_name;
  auto inner_config = Configuration::new_Configuration( inner_config_name );
  inner_solver_config = dynamic_cast< BlockSolverConfig * >( inner_config );

  if( ! inner_solver_config ) {
   inner_solver_config_file.close();
   delete( inner_config );
   continue;
  }

  try {
   inner_solver_config_file >> *inner_solver_config;
  }
  catch( ... ) {
   inner_solver_config_file.close();
   delete( inner_config );
   continue;
  }

  inner_solver_config_file.close();

  for( Index j = 0 ; j < inner_solver_config->num_ComputeConfig() ; ++j ) {
   if( inner_solver_config->get_SolverName( j ) == "LagrangianDualSolver" ) {
    delete( inner_config );
    return( true );
   }
  }
  delete( inner_config );
 }
 return( false );
}

/*--------------------------------------------------------------------------*/

void config_Lagrangian_dual( BlockSolverConfig * sddp_solver_config ,
                             SDDPBlock * sddp_block ) {

 if( sddp_block->get_number_nested_Blocks() == 0 )
  // The SDDPBlock has no sub-Block. There is nothing to be configured.
  return;

 BlockSolverConfig * inner_solver_config = nullptr;
 ComputeConfig * lagrangian_dual_compute_config = nullptr;
 ComputeConfig * compute_config = nullptr;

 // It indicates whether some Solver is a [Parallel]BundleSolver
 bool bundle_solver = false;
 bool do_easy_components = true;
 std::vector< int > vintNoEasy;

 // Index of the HydroSystemUnitBlock
 int hydro_system_index = -1;

 for( Index i = 0 ; i < sddp_solver_config->num_ComputeConfig() ; ++i ) {

  if( sddp_solver_config->get_SolverName( i ) != "SDDPSolver" &&
      sddp_solver_config->get_SolverName( i ) != "ParallelSDDPSolver" &&
      sddp_solver_config->get_SolverName( i ) != "SDDPGreedySolver" )
   continue;

  compute_config = sddp_solver_config->get_SolverConfig( i );

  // Check if strInnerBSC is present

  auto strInnerBSC = get_str_par( compute_config , "strInnerBSC" );

  if( strInnerBSC.empty() )
   return;

  // If it is, check if it is a config for a LagrangianDualSolver

  std::ifstream inner_solver_config_file
   ( config_filename_prefix + strInnerBSC , std::ifstream::in );

  if( ! inner_solver_config_file.is_open() )
   return;

  std::string inner_config_name;
  inner_solver_config_file >> eatcomments >> inner_config_name;
  auto inner_config = Configuration::new_Configuration( inner_config_name );
  inner_solver_config = dynamic_cast< BlockSolverConfig * >( inner_config );

  if( ! inner_solver_config ) {
   inner_solver_config_file.close();
   delete( inner_config );
   return;
  }

  try {
   inner_solver_config_file >> *inner_solver_config;
  }
  catch( ... ) {
   inner_solver_config_file.close();
   delete( inner_config );
   return;
  }

  inner_solver_config_file.close();

  for( Index j = 0 ; j < inner_solver_config->num_ComputeConfig() ; ++j ) {

   if( inner_solver_config->get_SolverName( j ) != "LagrangianDualSolver" )
    // It is not a ComputeConfig for a LagrangianDualSolver.
    // Check the next one.
    continue;

   lagrangian_dual_compute_config = inner_solver_config->get_SolverConfig( j );

   if( ! lagrangian_dual_compute_config )
    continue;

   // Find the inner Solver.
   auto sit = std::find_if( lagrangian_dual_compute_config->str_pars.begin() ,
                            lagrangian_dual_compute_config->str_pars.end() ,
                            []( auto & pair ) {
                             return( pair.first == "str_LDSlv_ISName" ); } );
   if( sit == lagrangian_dual_compute_config->str_pars.end() )
    // If it's not there, do nothing.
    continue;

   // Check if it is a [Parallel]BundleSolver.
   if( ( sit->second.find( "BundleSolver" ) == std::string::npos ) &&
       ( sit->second.find( "ParallelBundleSolver" ) == std::string::npos ) )
    continue;  // If it is not, do nothing.

   bundle_solver = true;

   // Check if the BundleSolver uses easy components.
   // Find if the ComputeConfig contains "intDoEasy".
   auto it = std::find_if( lagrangian_dual_compute_config->int_pars.begin() ,
                           lagrangian_dual_compute_config->int_pars.end() ,
                           []( auto & pair ) {
                            return( pair.first == "intDoEasy" ); } );
   if( it != lagrangian_dual_compute_config->int_pars.end() ) // if so
    do_easy_components = ( it->second & 1 ) > 0;  // read it
   else                               // otherwise
    do_easy_components = true;        // assume it is true (default)

   // We assume that there is at most one [Parallel]BundleSolver
   break;
  } // for each ComputeConfig for the inner Solver

  if( bundle_solver )
   break; // a BundleSolver has been found

 } // for each ComputeConfig for the Solver of SDDPBlock

 if( ! bundle_solver )
  // Since there is no BundleSolver, there is no need to configure any Block
  return;

 // The Configuration to be passed to get_var_solution() of the inner
 // Solver. We assume that only the HydroSystemBlock contains the necessary
 // part of the Solution (and that there is only one HydroSystemBlock) and
 // that the index of the HydroSystemBlock is the same at every stage.
 Configuration * get_var_solution_config = nullptr;

 // The Configuration to be passed to get_dual_solution() of the inner Solver.
 Configuration * get_dual_solution_config = nullptr;

 enum ConfigIndex { thermal = 0 , hydro , other_unit , default_config };

 // Vector with unique names of Configuration files ordered according to the
 // ConfigIndex enum.
 const std::vector< std::string > vstr_LDSl_Cfg = { thermal_config_filename ,
  hydro_config_filename , other_unit_config_filename ,
  default_config_filename };

 // We assume that all sub-Blocks of SDDPBlock have the same structure.

 const auto sub_block = sddp_block->get_nested_Block( 0 );

 auto stochastic_block = static_cast< StochasticBlock * >( sub_block );
 auto benders_block = static_cast< BendersBlock * >
  ( stochastic_block-> get_nested_Blocks().front() );
 auto objective = static_cast< FRealObjective * >
  ( benders_block->get_objective() );
 auto benders_function = static_cast< BendersBFunction * >
  ( objective->get_function() );
 auto inner_block = benders_function->get_inner_block();

 std::vector< int > vint_LDSl_WBSCfg;
 vint_LDSl_WBSCfg.reserve( inner_block->get_number_nested_Blocks() );

 /* The vector "required_primal_solution" will store the indices of Blocks
  * whose primal solutions are required (during the solution process). In
  * SDDP, only the primal solution of the HydroSystemUnitBlock is necessary
  * (as only the final volumes of the reservoirs are required during the
  * solution process). In simulation mode, the primal solutions that are
  * required are those of the Blocks that link two consecutive stages, which
  * are HydroSystemUnitBlock, BatteryUnitBlock, and ThermalUnitBlock.
  *
  * Notice that, in simulation mode, not all Blocks have their primal
  * solutions retrieved, which impacts the part of the solution that is output
  * (see UCBlockSolutionOutput). If the solutions of other Blocks are required
  * to be output when using LagrangianDualSolver+BundleSolver, then the
  * indices of these Blocks must be added to the vector
  * "required_primal_solution".
  *
  * This is currently not done due to a limitation of BundleSolver.
  * BundleSolver does not currently provide primal solutions for easy
  * components. Therefore, in order to have the primal solution of Blocks
  * other than HydroSystemUnitBlock, BatteryUnitBlock, and ThermalUnitBlock,
  * these Blocks must be treated as hard components (and they are currently
  * treated as easy components). Once BundleSolver is capable of providing
  * primal solutions of easy components, these Blocks can remain as easy
  * components and their indices can simply be added to the vector
  * "required_primal_solution". */

 std::vector< int > required_primal_solution;

 int inner_sub_block_index = 0;
 for( auto inner_sub_block : inner_block->get_nested_Blocks() ) {

  if( simulation_mode &&
      dynamic_cast< BatteryUnitBlock * >( inner_sub_block ) ) {

   required_primal_solution.push_back( inner_sub_block_index );

   // The primal solution of the BatteryUnitBlock is required as the storage
   // levels link two consecutive stages. Since BundleSolver currently does
   // not provide primal solutions for easy components, the BatteryUnitBlock
   // must be treated as a hard component. Once this feature is implemented by
   // BundleSolver, the BatteryUnitBlock can become an easy component.
   vint_LDSl_WBSCfg.push_back( ConfigIndex::other_unit );
   vintNoEasy.push_back( inner_sub_block_index );
  }
  if( dynamic_cast< ThermalUnitBlock * >( inner_sub_block ) ) {

   if( simulation_mode )
    required_primal_solution.push_back( inner_sub_block_index );

   // ThermalUnitBlock is a non-easy component since there is a specialized
   // solver for it.
   vint_LDSl_WBSCfg.push_back( ConfigIndex::thermal );
   vintNoEasy.push_back( inner_sub_block_index );
  }
  else if( dynamic_cast< HydroSystemUnitBlock * >( inner_sub_block ) ) {
   required_primal_solution.push_back( inner_sub_block_index );
   hydro_system_index = inner_sub_block_index;

   // The HydroSystemUnitBlock could be treated as an easy component. However,
   // due to a current limitation of BundleSolver, the HydroSystemUnitBlock is
   // considered a hard component. This is because its primal solution (the
   // volume of the reservoirs) is required both in SDDP and in simulation
   // mode, but BundleSolver cannot currently provide primal solutions for
   // easy components. Once this feature is implemented by BundleSolver, the
   // HydroSystemUnitBlock can become an easy component.

   vint_LDSl_WBSCfg.push_back( ConfigIndex::hydro );
   vintNoEasy.push_back( inner_sub_block_index );
  }
  else if( ( simulation_mode || force_hard_components ) &&
           dynamic_cast< IntermittentUnitBlock * >( inner_sub_block ) ) {

   if( simulation_mode )
    required_primal_solution.push_back( inner_sub_block_index );

   vint_LDSl_WBSCfg.push_back( ConfigIndex::other_unit );
   vintNoEasy.push_back( inner_sub_block_index );
  }
  else if( ( simulation_mode || force_hard_components ) &&
           dynamic_cast< NetworkBlock * >( inner_sub_block ) ) {
   // The dual solution of the NetworkBlock is part of the required output of
   // the simulation. Since BundleSolver currently does not provide solutions
   // for easy components, the NetworkBlock must be treated as a hard
   // component. Once this feature is implemented by BundleSolver, the
   // NetworkBlock can become an easy component.
   vint_LDSl_WBSCfg.push_back( ConfigIndex::default_config );
   vintNoEasy.push_back( inner_sub_block_index );

   if( simulation_mode )
    required_primal_solution.push_back( inner_sub_block_index );
  }
  else if( ! do_easy_components ) {
   if( simulation_mode )
    required_primal_solution.push_back( inner_sub_block_index );

   vintNoEasy.push_back( inner_sub_block_index );
   if( dynamic_cast< UnitBlock * >( inner_sub_block ) )
    vint_LDSl_WBSCfg.push_back( ConfigIndex::other_unit );
   else
    vint_LDSl_WBSCfg.push_back( ConfigIndex::default_config );
  }
  else
   vint_LDSl_WBSCfg.push_back( ConfigIndex::default_config );

  ++inner_sub_block_index;
 }

 if( ! vintNoEasy.empty() ) {
  // Remove any vintNoEasy parameter that is possibly there
  lagrangian_dual_compute_config->vint_pars.erase
   ( std::remove_if( lagrangian_dual_compute_config->vint_pars.begin() ,
                     lagrangian_dual_compute_config->vint_pars.end() ,
                     []( const auto & pair ) {
                      return( pair.first == "vintNoEasy" ); } ) ,
     lagrangian_dual_compute_config->vint_pars.end() );

  // Add the vintNoEasy parameter that was constructed here
  lagrangian_dual_compute_config->vint_pars.push_back
   ( std::make_pair( "vintNoEasy" , std::move( vintNoEasy ) ) );
 }

 lagrangian_dual_compute_config->vint_pars.push_back
  ( std::make_pair( "vint_LDSl_WBSCfg" , std::move( vint_LDSl_WBSCfg ) ) );

 lagrangian_dual_compute_config->vstr_pars.push_back
  ( std::make_pair( "vstr_LDSl_Cfg" , std::move( vstr_LDSl_Cfg ) ) );

 // Configuration for the sub-Blocks may need to be cloned since the same
 // Configuration is used to configure multiple Blocks.
 lagrangian_dual_compute_config->int_pars.push_back
  ( std::make_pair( "int_LDSlv_CloneCfg" , 1 ) );

 compute_config->str_pars.erase
  ( std::remove_if( compute_config->str_pars.begin() ,
                    compute_config->str_pars.end() ,
                    []( const auto & pair ) {
                     return( pair.first == "strInnerBSC" ); } ) ,
    compute_config->str_pars.end() );

 /* The extra Configuration of the SDDPSolver and the SDDPGreedySolver is a
  * vector with pointers to the following elements (in that order):
  *
  * - a BlockConfig (which is currently nullptr) for the inner Block;
  *
  * - a BlockSolverConfig for the inner Block;
  *
  * - the Configuration to be passed to get_var_solution() when retrieving
  *   the Solutions to the inner Blocks of the BendersBFunctions.
  *
  * The extra Configuration of the SDDPGreedySolver has an additional (fourth)
  * element, which is
  *
  * - the Configuration to be passed to get_dual_solution() when retrieving
  *   the dual Solutions to the inner Blocks of the BendersBFunctions. */

 Configuration * extra_config = nullptr;

 /* Here we create a Configuration for
  * LagrangianDualSolver::get_var_solution() that requires the primal
  * solutions only of certain Blocks. In SDDP, only the primal solution of the
  * HydroSystemUnitBlock is necessary (as only the final volumes of the
  * reservoirs are required during the solution process). In simulation mode,
  * the solutions that are required are those of the Blocks that link two
  * consecutive stages, which are HydroSystemUnitBlock, BatteryUnitBlock, and
  * ThermalUnitBlock. */

 get_var_solution_config = new SimpleConfiguration< std::vector< int > >
  ( required_primal_solution );

 if( simulation_mode ) {
  /* In simulation mode, the only part of the dual Solution that is required
   * is that associated with the linking constraints (the set of Constraint
   * defined in the UCBlock). Therefore, we create a Configuration for the
   * get_dual_solution() method that ignores the dual solutions of the
   * sub-Blocks of the UCBlock and requires the dual solutions of the linking
   * constraints. */

  get_dual_solution_config = new SimpleConfiguration
   < std::vector< std::pair< int , int > > >
   ( { std::make_pair< int , int >( -1 , -1 ) } );

  // Create the extra Configuration for SDDPGreedySolver.

  extra_config = new SimpleConfiguration< std::vector< Configuration * > >
   ( { nullptr , inner_solver_config , get_var_solution_config ,
      get_dual_solution_config } );
 }
 else {
  // Create the extra Configuration for SDDPSolver.
  extra_config = new SimpleConfiguration< std::vector< Configuration * > >
   ( { nullptr , inner_solver_config , get_var_solution_config } );
 }

 compute_config->f_extra_Configuration = extra_config;


 if( ( ! simulation_mode ) && ( hydro_system_index >= 0 ) ) {
  // Configure all BendersBFunction to retrieve the right portion of the dual
  // variables.

  // In SDDP, only the dual variables of the component defined by the
  // HydroSystemUnitBlock are needed, as all constraints handled by the
  // BendersBFunction belong to it.
  auto get_dual_config =
   new SimpleConfiguration< std::vector< std::pair< int , Configuration * > > >
   ( { std::make_pair( hydro_system_index , nullptr ) } );

  auto benders_function_config = new ComputeConfig;

  // Differential mode to keep the previous configuration.
  benders_function_config->f_diff = true;

  benders_function_config->f_extra_Configuration =
   new SimpleConfiguration< std::map< std::string , Configuration * > >
   ( { { "get_dual" , get_dual_config  } ,
       { "get_dual_partial" , get_dual_config->clone() } } );

  for( auto sub_block : sddp_block->get_nested_Blocks() ) {
   auto stochastic_block = static_cast< StochasticBlock * >( sub_block );
   auto benders_block = static_cast< BendersBlock * >
    ( stochastic_block-> get_nested_Blocks().front() );
   auto objective = static_cast< FRealObjective * >
    ( benders_block->get_objective() );
   auto benders_function = static_cast< BendersBFunction * >
    ( objective->get_function() );
   benders_function->set_ComputeConfig( benders_function_config );
  }

  delete( benders_function_config );
 }

 // OSIMPSolver is currently not able to deal with some changes in a Block
 // (for instance, when some bound structure changes). In order to try to
 // avoid this case, we set a scenario, so that when OSIMPSolver is attached
 // to a Block, the data in that Block is a relevant one and, hopefully, will
 // not later be responsible for any other change in the bound structure. If
 // OSIMPSolver still complains, then other actions may be required (for
 // instance, replacing zeros by very small numbers in the scenarios).

 for( Index t = 0 ; t < sddp_block->get_time_horizon() ; ++t )
  for( Index i = 0 ; i < sddp_block->get_num_sub_blocks_per_stage() ; ++i )
   sddp_block->set_scenario( 0 , t , i );
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
 auto solver_config = load_BlockSolverConfig( solver_config_filename );
 if( ! solver_config ) {
  block_solver_config_provided = false;
  solver_config = build_BlockSolverConfig();
 }

 auto cleared_solver_config = solver_config->clone();
 cleared_solver_config->clear();

 const auto is_using_lagrangian_dual_solver =
  using_lagrangian_dual_solver( solver_config );

 if( is_using_lagrangian_dual_solver && using_thermal_dp_solver
     ( config_filename_prefix + thermal_config_filename ) )
  // The ThermalUnitDPSolver cannot currently deal with spinning
  // reserves. Thus, any reserve that is provided must be ignored.
  ThermalUnitBlock::ignore_reserve();

 // For each Block descriptor
 for( auto block_description : blocks ) {

  // Deserialize the SDDPBlock

  auto sddp_block = new SDDPBlock;
  sddp_block->set_num_sub_blocks_per_stage( num_sub_blocks_per_stage );
  sddp_block->deserialize( block_description.second );

  // Configure the SDDPBlock

  if( given_block_config )
   given_block_config->apply( sddp_block );
  else {
   configure_Blocks( sddp_block , is_using_lagrangian_dual_solver ,
                     feasibility_tolerance , relative_violation ,
                     is_using_lagrangian_dual_solver );

   if( ! block_solver_config_provided ) {
    block_config = build_BlockConfig( sddp_block );
    block_config->apply( sddp_block );
    block_config->clear();
   }
  }

  // Configure the Solver

  if( is_using_lagrangian_dual_solver )
   config_Lagrangian_dual( solver_config , sddp_block );

  solver_config->apply( sddp_block );

  // Set the output stream for the log of the inner Solvers

  set_log( sddp_block , &std::cout );

  // Load possibly given cuts

  if( ! simulation_mode )
   load_cuts( sddp_block );

  // Eliminate redundant cuts if it is desired

  if( eliminate_redundant_cuts )
   CutProcessing().remove_redundant_cuts( sddp_block );

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

/// returns the final state (solution) of the system at the given \p stage
std::vector< double > get_final_state( SDDPBlock * block , Index stage ) {

 Index state_size = 0;
 for( Index i = 0 ; i < block->get_num_polyhedral_function_per_sub_block() ;
      ++i ) {
  state_size +=
   block->get_polyhedral_function( stage , i )->get_num_active_var();
 }

 std::vector< double > state;
 state.reserve( state_size );

 for( Index i = 0 ; i < block->get_num_polyhedral_function_per_sub_block() ;
      ++i ) {
  const auto polyhedral_function = block->get_polyhedral_function( stage , i );
  for( const auto & variable : * polyhedral_function ) {
   state.push_back
    ( static_cast< const ColVariable & >( variable ).get_value() );
  }
 }
 return( state );
}

/*--------------------------------------------------------------------------*/

void multiple_simulations( const netCDF::NcFile & file ) {
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
 auto solver_config = load_BlockSolverConfig( solver_config_filename );
 if( ! solver_config ) {
  block_solver_config_provided = false;
  solver_config = build_BlockSolverConfig();
 }

 auto cleared_solver_config = solver_config->clone();
 cleared_solver_config->clear();

 const auto is_using_lagrangian_dual_solver =
  using_lagrangian_dual_solver( solver_config );

 if( is_using_lagrangian_dual_solver && using_thermal_dp_solver
     ( config_filename_prefix + thermal_config_filename ) )
  // The ThermalUnitDPSolver cannot currently deal with spinning
  // reserves. Thus, any reserve that is provided must be ignored.
  ThermalUnitBlock::ignore_reserve();

 // For each Block descriptor
 for( auto block_description : blocks ) {

  // Simulate

  std::mt19937 random_number_engine;
  std::vector< double > initial_state;

  for( long i = 0 ; i < number_simulations ; ++i ) {

   std::cout << "Simulation " << i << "." << std::endl;

   /* In the simulation, Blocks of two consecutive stages are linked in such a
    * way that the final state of the system at one stage affects the system
    * at the next stage. For example, the initial volumes of the reservoirs
    * at the first time step of a stage must be equal to those at the last
    * time step of the previous stage.
    *
    * Once the UCBlock associated with some stage has been solved, the final
    * state of the system is retrieved and used to change the data of the
    * Blocks associated with the next stage (see the callback() function).
    *
    * Part of the data of some Blocks, however, cannot be changed after their
    * abstract representations have been generated. That is why, in simulation
    * (SDDPGreedySolver), the UCBlock is Solver-configured after the data
    * linking two consecutive stages is set, which occurs just before the
    * UCBlock is solved.
    *
    * An example of this data is the time a thermal unit has been on or off
    * (ThermalUnitBlock::set_init_updown_time), which is not allowed to be
    * changed after the abstract representation of the ThermalUnitBlock has
    * been generated.
    *
    * This prevents us from reusing the same Blocks in different simulations
    * and, thus, the Blocks are created at the beginning of each
    * simulation. */

   // Deserialize the SDDPBlock

   auto sddp_block = new SDDPBlock;
   sddp_block->set_num_sub_blocks_per_stage( num_sub_blocks_per_stage );
   sddp_block->deserialize( block_description.second );

   const bool set_initial_state = ( initial_solution_stage >= 0 ) &&
    ( initial_solution_stage < sddp_block->get_time_horizon() );

   // Configure the SDDPBlock

   if( given_block_config )
    given_block_config->apply( sddp_block );
   else {
    configure_Blocks( sddp_block , is_using_lagrangian_dual_solver ,
                      feasibility_tolerance , relative_violation ,
                      is_using_lagrangian_dual_solver );

    if( ! block_solver_config_provided ) {
     block_config = build_BlockConfig( sddp_block );
     block_config->apply( sddp_block );
     block_config->clear();
    }
   }

   // Configure the Solver

   if( is_using_lagrangian_dual_solver )
    config_Lagrangian_dual( solver_config , sddp_block );

   solver_config->apply( sddp_block );

   // Set the output stream for the log of the inner Solvers

   set_log( sddp_block , &std::cout );

   // Set some parameters of SDDPGreedySolver

   auto solver = dynamic_cast< SDDPGreedySolver * >
    ( sddp_block->get_registered_solvers().front() );

   if( ! solver )
    throw( std::logic_error( "The Solver for the SDDPBlock must be a "
                             "SDDPGreedySolver in simulation mode." ) );

   if( solver->get_int_par( solver->int_par_str2idx( "intLogVerb" ) ) )
    solver->set_log( & std::cout );

   auto subgradients_filename_prefix =
    solver->get_str_par( SDDPGreedySolver::strSimulationData );

   solver->set_callback( [ sddp_block ]( Index stage ) {
    callback( sddp_block , stage );
   });

   if( i > 0 ) {
    // Set the random number engine
    solver->set_random_number_engine( random_number_engine );

    if( set_initial_state ) {
     // If required, set the initial state parameter of SDDPGreedySolver as
     // the final solution of the last iteration.
     solver->set_par( SDDPGreedySolver::vdblInitialState ,
                      std::move( initial_state ) );
    }
   }

   // Load possibly given cuts

   if( ! cuts_filename.empty() )
    solver->set_par( SDDPGreedySolver::strLoadCuts , cuts_filename );

   // Eliminate redundant cuts if it is desired

   if( eliminate_redundant_cuts )
    CutProcessing().remove_redundant_cuts( sddp_block );

   // Set the name of the file that will output the subgradients

   if( ! subgradients_filename_prefix.empty()  )
    solver->set_par( SDDPGreedySolver::strSimulationData ,
                     subgradients_filename_prefix + "." + std::to_string( i ) );

   // Try to solve the SDDPBlock

   while( true ) {

    // Simulate
    const auto status = solver->compute();

    if( solver->has_var_solution() ) {
     // A feasible solution has been found

     if( set_initial_state ) {
      // Retrieve the solution
      solver->get_var_solution();
      initial_state = get_final_state( sddp_block , initial_solution_stage );
     }

     // Save the random number engine
     random_number_engine = solver->get_random_number_engine();

     // Output simulation status
     show_simulation_status( status , solver->get_fault_stage() );
     const auto lb = solver->get_lb();
     const auto ub = solver->get_ub();
     std::cout << "Lower bound: " << std::setprecision( 20 ) << lb << std::endl;
     std::cout << "Upper bound: " << std::setprecision( 20 ) << ub << std::endl;

     break;
    }
   }

   // Destroy the SDDPBlock and the Configurations

   if( block_config )
    block_config->apply( sddp_block );
   if( ! given_block_config ) {
    delete( block_config );
    block_config = nullptr;
   }

   cleared_solver_config->apply( sddp_block );
   delete( sddp_block );

  }
 }

 delete( block_config );
 delete( given_block_config );
 delete( solver_config );
 delete( cleared_solver_config );
}

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv ) {

#ifdef USE_MPI
 boost::mpi::environment env(argc, argv);
#endif

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

   if( simulation_mode && ( number_simulations > 1 ) )
    multiple_simulations( file );
   else
    process_block_file( file );

   break;
  }

  default:
   std::cerr << filename << " is not a valid SMS++ file." << std::endl;
   exit( 1 );
 }

 file.close();
 return( 0 );
}
