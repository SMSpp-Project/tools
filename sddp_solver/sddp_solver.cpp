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
 * followed by the path to the netCDF file containing the initial cuts, in
 * one of the formats supported by SDDPBlock::deserialize_cuts(), i.e.,
 * either the netCDF or the historical CSV one (automatically detected);
 * the final cuts are output in both formats, see the
 * BellmanValues[All]OUT.nc4 and BellmanValues[All]OUT.csv files produced
 * in the directory specified by the -d option.
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
 * subproblem associated with stage of a simulation will serve as the
 * initial state for the first stage subproblem of the next simulation. If
 * STAGE does not belong to that interval (that is, if it is negative or
 * greater than or equal to T) or if the -t option is not used, then no
 * changes are made to the way the initial state is specified.
 *
 * \author Rafael Durbano Lobato \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Rafael Durbano Lobato, Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "common_utils.h"

#include <filesystem>
#include <iomanip>
#include <queue>

#include <BatteryUnitBlock.h>
#include <BendersBlock.h>
#include <BlockSolverConfig.h>
#include <IntermittentUnitBlock.h>
#include <SlackUnitBlock.h>
#include <ThermalUnitBlock.h>
#include <HydroSystemUnitBlock.h>
#include <PolyhedralFunctionBlock.h>
#include <RBlockConfig.h>
#include <SDDPBlock.h>
#include <StochasticBlock.h>
#include <SDDPGreedySolver.h>
#include <SDDPSolver.h>
#include <UCBlock.h>

#include <CutProcessing.h>

#include "SDDPBlockSolutionOutput.h"

#ifdef USE_MPI
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#endif

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

std::string output_solution_directory = ".";
std::string cuts_filename {};
std::string cut_processing_sconf_file {};

long scenario_id = 0;
long num_sub_blocks_per_stage = 1;
long number_simulations = 1;
long initial_solution_stage = -1;
bool simulation_mode = false;
bool eliminate_redundant_cuts = false;

const bool force_hard_components = false;

// If hydro_is_easy_component is true (see the -z option), the
// HydroSystemUnitBlock is treated as an easy component by the
// [Parallel]BundleSolver; its primal solution (which is needed, as the
// volume of the reservoirs links two consecutive stages) is then recovered
// from the master problem solution via the Configuration in
// get_var_solution_bundle.txt
bool hydro_is_easy_component = false;

const std::string get_var_solution_bundle_filename =
                                    "config/get_var_solution_bundle.txt";

// If set_polyhedral_function_bound is true, the PolyhedralFunction of each
// stage (the Bellman, cost-to-go function) gets the finite global lower
// bound below: since the stage costs are non-negative, 0 is a valid bound,
// and a bounded PolyhedralFunction keeps the stage subproblems bounded even
// when no (initial) cut is available
constexpr bool set_polyhedral_function_bound = true;
constexpr double polyhedral_function_bound = 0;

// Name of the files containing the default (meta) BlockConfig for the inner
// Block of each stage and the default BlockSolverConfig for the inner Block
// of each BendersBFunction; see configure_Blocks() and build_BlockConfig().
// As every configuration file, they are resolved against the directory the
// tool is run from, as in the other tools
// note: these are resolved against the -c prefix, which defaults to
// "config/" (see main()); the file names referenced INSIDE configuration
// files are instead resolved by the loading Solver against the directory
// the tool is run from, hence they carry an explicit "config/"
const std::string default_block_config_filename = "SDDPBCfg.txt";
const std::string default_block_config_filename_LD = "SDDPBCfg-LD.txt";
const std::string benders_solver_config_filename = "BendersBSCfg.txt";

/*--------------------------------------------------------------------------*/

const std::string my_short_opts = "d:e:l:n:i:m:rst:z";

const std::vector< option > my_long_opts = {
  { "output-dir" ,               required_argument , nullptr , 'd' } ,
  { "eliminate-redundant-cuts" , required_argument , nullptr , 'e' } ,
  { "load-cuts" ,                required_argument , nullptr , 'l' } ,
  { "num-blocks" ,               required_argument , nullptr , 'n' } ,
  { "scenario" ,                 required_argument , nullptr , 'i' } ,
  { "num-simulations" ,          required_argument , nullptr , 'm' } ,
  { "relax" ,                    no_argument ,       nullptr , 'r' } ,
  { "simulation" ,               no_argument ,       nullptr , 's' } ,
  { "stage" ,                    required_argument , nullptr , 't' } ,
  { "hydro-easy" ,               no_argument ,       nullptr , 'z' }
  };

const std::string my_help =
 "  -d, --output-dir                directory where solution and cuts files\n"
 "                                  are written\n"
 "  -e, --eliminate-redundant-cuts  eliminate given redundant cuts\n"
 "  -l, --load-cuts <file>          load cuts from a file\n"
 "  -n, --num-blocks <number>       number of sub-Blocks per stage\n"
 "  -i, --scenario <index>          the index of the scenario\n"
 "  -m, --num-simulations <number>  number of simulations to be performed\n"
 "  -s, --simulation                simulation mode\n"
 "  -t, --stage <stage>             stage from which initial state is taken\n"
 "  -z, --hydro-easy                treat the hydro system as an easy component";

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 switch( opt ) {  // non-standard options
  case 'd': output_solution_directory = std::string( optarg ); return( true );
  case 'e': cut_processing_sconf_file = std::string( optarg );
            eliminate_redundant_cuts = true;
            return( true );
  case 'i': scenario_id = get_long_option();
            if( scenario_id < 0 ) {
             std::cerr << "scenario index  must be a nonnegative integer"
                       << std::endl;
             exit( 1 );
             }
            return( true );
  case 'l': cuts_filename = std::string( optarg ); return( true );
  case 'm': number_simulations = get_long_option();
            if( number_simulations < 1 ) {
             std::cerr << "number of simulations must be at least 1"
                       << std::endl;
             exit( 1 );
             }
            return( true );
  case 'n': num_sub_blocks_per_stage = get_long_option();
            if( num_sub_blocks_per_stage <= 0 ) {
             std::cout << "number of sub-Blocks per stage must be a "
                       << "positive integer" << std::endl;
             exit( 1 );
             }
            return( true );
  case 'r':
   std::cout << "The -r option no longer exists. In order relax the "
             << "integrality constraints,\nplease properly configure the "
             << "solver. For instance, some solvers have the\nparameter "
             << "'intRelaxIntVars', which can be set to 1 in the solver\n"
             << "configuration file associated with the Block whose "
             << "constraints must be\nrelaxed." << std::endl;
   exit( 1 );
  case 's': simulation_mode = true; return( true );
  case 'z': hydro_is_easy_component = true; return( true );
  case 't': initial_solution_stage = get_long_option(); return( true );
  case '?':
  default:  return( false );
  }
 }

/*--------------------------------------------------------------------------*/

std::string get_cut_processing_solver_config_filepath()
{
 return( cut_processing_sconf_file );
 }

/*--------------------------------------------------------------------------*/

Block * get_uc_block( const SDDPBlock * sddp_block , const Index stage ,
                      const Index sub_block_index = 0 )
{
 auto benders_block = static_cast< BendersBlock * >(
                       sddp_block->get_sub_Block( stage , sub_block_index )->
                       get_inner_block() );

 auto objective = static_cast< FRealObjective * >(
                                            benders_block->get_objective() );

 auto benders_function = static_cast< BendersBFunction * >(
                                                 objective->get_function() );

 return( benders_function->get_inner_block() );
 }

/*--------------------------------------------------------------------------*/

bool update_hydro_unit( Block * previous_block , Block * block ,
                        Index stage )
{
 auto unit = dynamic_cast< HydroUnitBlock * >( block );
 auto previous_unit = dynamic_cast< HydroUnitBlock * >( previous_block );

 if( ( ! unit ) && ( ! previous_unit ) )
  return( false );

 if( ( ! unit ) || ( ! previous_unit ) )
  throw( std::logic_error(
           "sddp_solver: UCBlocks at stages " + std::to_string( stage - 1 ) +
           " and " + std::to_string( stage ) +
           " do not have the same structure" ) );

 auto number_generators = previous_unit->get_number_generators();

 if( number_generators != unit->get_number_generators() )
  throw( std::logic_error(
           "sddp_solver: HydroUnitBlock at stage " +
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
                          Index stage )
{
 auto unit = dynamic_cast< BatteryUnitBlock * >( block );
 auto previous_unit = dynamic_cast< BatteryUnitBlock * >( previous_block );

 if( ( ! unit ) && ( ! previous_unit ) )
  return( false );

 if( ( ! unit ) || ( ! previous_unit ) )
  throw( std::logic_error(
           "sddp_solver: UCBlocks at stages " + std::to_string( stage - 1 ) +
           " and " + std::to_string( stage ) +
           " do not have the same structure." ) );

 const auto time_horizon = previous_unit->get_time_horizon();

 std::vector< double > initial_power_data = {
  ( previous_unit->get_active_power( 0 ) + time_horizon - 1 )->get_value() };

 unit->set_initial_power( initial_power_data.cbegin() );

 std::vector< double > initial_storage_data =
  { previous_unit->get_storage_level()[ time_horizon - 1 ].get_value() };

 unit->set_initial_storage( initial_storage_data.cbegin() );

 return( true );
 }

/*--------------------------------------------------------------------------*/

int compute_init_up_down_time( const SDDPBlock * sddp_block ,
                               ThermalUnitBlock * previous_unit ,
                               ThermalUnitBlock * unit , Index stage ,
                               Index sub_block_index )
{
 auto time_horizon = previous_unit->get_time_horizon();
 auto commitment = previous_unit->get_commitment( 0 ) + time_horizon - 1;

 auto shutdown = previous_unit->get_shut_down( time_horizon - 1 );
 if( shutdown && shutdown->get_value() >= 0.5 )
  return( 0 );

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
   auto uc_block = get_uc_block( sddp_block , stage , sub_block_index );
   path.build( unit , uc_block );
   }

  auto previous_uc_block = get_uc_block( sddp_block , stage - outer_t - 2 ,
                                         sub_block_index );
  previous_unit = dynamic_cast< ThermalUnitBlock * >(
                           path.get_element< Block >( previous_uc_block ) );

  time_horizon = previous_unit->get_time_horizon();

  if( ! previous_unit )
   throw( std::logic_error(
            "sddp_solver::update_thermal_block: ThermalUnitBlock not found "
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
                          Index stage , Index sub_block_index )
{
 auto previous_unit = dynamic_cast< ThermalUnitBlock * >( previous_block );
 auto unit = dynamic_cast< ThermalUnitBlock * >( block );

 if( ( ! unit ) && ( ! previous_unit ) )
  return( false );

 if( ( ! unit ) || ( ! previous_unit ) )
  throw( std::logic_error(
          "sddp_solver: UCBlocks at stages " + std::to_string( stage - 1 ) +
          " and " + std::to_string( stage ) +
          " do not have the same structure" ) );

 if( simulation_mode ) {
  auto init_up_down_time = compute_init_up_down_time(
           sddp_block , previous_unit , unit , stage , sub_block_index );

  std::vector< int > init_up_down_time_data = { init_up_down_time };
  unit->set_init_updown_time( init_up_down_time_data.cbegin() );
  }

 const auto time_horizon = previous_unit->get_time_horizon();

 std::vector< double > active_power_data = {
  ( previous_unit->get_active_power( 0 ) + time_horizon - 1 )->get_value() };
 unit->set_initial_power( active_power_data.cbegin() );

 return( true );
 }

/*--------------------------------------------------------------------------*/

void callback( SDDPBlock * sddp_block , Block::Index stage ,
               Block::Index sub_block_index )
{
 if( stage == 0 )
  return;

 auto previous_uc_block = get_uc_block( sddp_block , stage - 1 ,
                                        sub_block_index );
 auto uc_block = get_uc_block( sddp_block , stage , sub_block_index );

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

  if( n != previous_block->get_number_nested_Blocks() )
   throw( std::logic_error(
           "sddp_solver: UCBlocks at stages " + std::to_string( stage - 1 ) +
           " and " + std::to_string( stage ) +
           " do not have the same structure" ) );

  for( decltype( n ) i = 0 ; i < n ; ++i ) {
   blocks.push( block->get_nested_Block( i ) );
   previous_blocks.push( previous_block->get_nested_Block( i ) );
   }

  update_hydro_unit( previous_block , block , stage )
   || update_thermal_unit( sddp_block , previous_block , block , stage ,
                           sub_block_index )
   || update_battery_unit( previous_block , block , stage );
  }
 }

/*--------------------------------------------------------------------------*/

static void show_sddp_greedy_status( Block::Index status ,
                                     Block::Index fault_stage )
{
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
             << " terminated due to a time limit." << std::endl;
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

void simulate( SDDPBlock * sddp_block )
{
 auto solver = dynamic_cast< SDDPGreedySolver * >(
                             sddp_block->get_registered_solvers().front() );
 if( ! solver )
  throw( std::logic_error( "The Solver for the SDDPBlock must be a "
                           "SDDPGreedySolver in simulation mode" ) );

 // the (index of the) sub-Block of each stage the simulation works on
 const auto sub_block_index = Index( solver->get_int_par(
                                     SDDPGreedySolver::intSubBlockIndex ) );

 solver->set_callback( [ sddp_block , sub_block_index ]( Index stage ) {
                        callback( sddp_block , stage , sub_block_index );
                        } );

 // Load possibly given cuts
 if( ! cuts_filename.empty() )
  solver->set_par( SDDPGreedySolver::strLoadCuts , cuts_filename );

 // Eliminate redundant cuts if it is desired
 if( eliminate_redundant_cuts )
  CutProcessing( get_blocksolverconfig(
                             get_cut_processing_solver_config_filepath() )
                 ).remove_redundant_cuts( sddp_block );

 solver->set_scenario_id( scenario_id );

 // load the given State (the cuts), if provided - - - - - - - - - - - - - - -
 get_initial_State( solver );

 auto status = solver->compute();

 #ifdef USE_MPI
  boost::mpi::communicator world;
  if( world.rank() == 0 ) {
 #endif
   show_sddp_greedy_status( status , solver->get_fault_stage() );

   // the solution is also output in the historical CSV files (one per
   // quantity, suffixed with the scenario index), kept so that existing
   // consumers keep working unchanged
   SDDPBlockSolutionOutput output( output_solution_directory );

   if( solver->has_var_solution() ) {
    solver->get_var_solution();

    output.print( sddp_block , scenario_id , true );

    // write final Solution, if required
    write_final_Solution( sddp_block );
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

void show_status( Index status )
{
 switch( status ) {
  case( SDDPSolver::kOK ) :
   std::cout << "Optimal solution found" << std::endl;
   break;

 case( SDDPSolver::kError ) :
  std::cout << "Error" << std::endl;
  break;

 case( SDDPSolver::kUnbounded ) :
  std::cout << "A subproblem is unbounded" << std::endl;
  break;

 case( SDDPSolver::kInfeasible ) :
  std::cout << "A subproblem is infeasible" << std::endl;
  break;

 case( SDDPSolver::kStopTime ) :
  std::cout << "Terminated due to time limit" << std::endl;
  break;

 case( SDDPSolver::kStopIter ) :
  std::cout << "Terminated due to iter limit" << std::endl;
  break;
  }
 }

/*--------------------------------------------------------------------------*/
/// serialize the cuts of the given SDDPBlock in the output directory

static void serialize_cuts( const SDDPBlock * sddp_block ,
                            const std::string & filename )
{
 sddp_block->serialize_cuts( ( std::filesystem::path( output_solution_directory )
                               / filename ).string() );
 }

/*--------------------------------------------------------------------------*/

void solve( SDDPBlock * sddp_block )
{
 // retrieve SDDPSolver- - - - - - - - - - - - - - - - - - - - - - - - - - - -
 auto solver = dynamic_cast< SDDPSolver * >(
                               sddp_block->get_registered_solvers().front() );
 if( ! solver )
  throw( std::logic_error( "The Solver for the SDDPBlock must be a "
                           "SDDPSolver in optimization mode" ) );

 solver->set_log( &std::cout );

 // set initial Solution, if provided - - - - - - - - - - - - - - - - - - - -
 get_initial_Solution( sddp_block );

 // load the given State, if provided - - - - - - - - - - - - - - - - - - - -
 get_initial_State( solver );

 // solve the stochastic problem - - - - - - - - - - - - - - - - - - - - - - -
 if( ! dryrun ) {
  auto status = solver->compute();
  show_status( status );
  }

 // write final Solution, if required - - - - - - - - - - - - - - - - - - - -
 write_final_Solution( sddp_block );

 // write final State, if required- - - - - - - - - - - - - - - - - - - - - -
 write_final_State( solver );

 // output final cuts- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 serialize_cuts( sddp_block , "BellmanValuesAllOUT.nc4" );
 // also in the historical CSV format, kept so that existing consumers
 // keep working unchanged
 serialize_cuts( sddp_block , "BellmanValuesAllOUT.csv" );

 if( eliminate_redundant_cuts )
  CutProcessing( get_blocksolverconfig(
                              get_cut_processing_solver_config_filepath() )
                 ).remove_redundant_cuts( sddp_block );

 serialize_cuts( sddp_block , "BellmanValuesOUT.nc4" );
 serialize_cuts( sddp_block , "BellmanValuesOUT.csv" );

 }  // end( solve )

/*--------------------------------------------------------------------------*/

void configure_Blocks( SDDPBlock * sddp_block ,
                       bool is_using_lagrangian_dual_solver )
{
 // possibly give a finite global bound to the PolyhedralFunction of every
 // stage; see the comments to set_polyhedral_function_bound
 if( set_polyhedral_function_bound )
  for( Index stage = 0 ; stage < sddp_block->get_time_horizon() ; ++stage )
   for( Index j = 0 ; j < sddp_block->get_num_sub_blocks_per_stage() ; ++j ) {

    std::queue< Block * > blocks;
    blocks.push( get_uc_block( sddp_block , stage , j ) );

    while( ! blocks.empty() ) {
     auto block = blocks.front();
     blocks.pop();
     for( auto inner : block->get_nested_Blocks() )
      blocks.push( inner );

     if( auto polyhedral = dynamic_cast< PolyhedralFunctionBlock * >( block ) )
      polyhedral->get_PolyhedralFunction().modify_bound(
                                                polyhedral_function_bound );
     }
    }

 /* The default configuration of the inner Block of each stage is entirely
  * described by a "meta" BlockConfig file (a map from Block classname() to
  * the BlockConfig to be applied to every Block of that class, with "*" as
  * the default), dispatched by the common config_Block() machinery; see
  * SDDPBCfg.txt and SDDPBCfg-LD.txt. */

 auto block_config = get_config( is_using_lagrangian_dual_solver ?
                                 default_block_config_filename_LD :
                                 default_block_config_filename );

 if( ! block_config ) {
  std::cout << "Warning: default BlockConfig file "
            << ( is_using_lagrangian_dual_solver ?
                 default_block_config_filename_LD :
                 default_block_config_filename )
            << " not found; the inner Blocks are not configured"
            << std::endl;
  return;
  }

 for( Index stage = 0 ; stage < sddp_block->get_time_horizon() ; ++stage )
  for( Index j = 0 ; j < sddp_block->get_num_sub_blocks_per_stage() ; ++j )
   config_Block( get_uc_block( sddp_block , stage , j ) , block_config ,
                 nullptr );

 delete block_config;
 }

/*--------------------------------------------------------------------------*/

void set_log( SDDPBlock * sddp_block , std::ostream * output_stream )
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
  for( auto solver : inner_block->get_registered_solvers() )
   if( solver )
    solver->set_log( output_stream );
  }
 }

/*--------------------------------------------------------------------------*/

void process_prob_file( const netCDF::NcFile & file )
{
 auto problems = file.getGroups();
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
  auto block_config = static_cast< BlockConfig * >(
                      BlockConfig::new_Configuration( block_config_group ) );
  if( ! block_config )
   throw( std::logic_error( "BlockConfig group was not properly provided" ) );
  block_config->apply( sddp_block );
  block_config->clear();

  // Configure solver
  auto solver_config_group = problem_group.getGroup( "BlockSolver" );
  auto block_solver_config = static_cast< BlockSolverConfig * >(
               BlockSolverConfig::new_Configuration( solver_config_group ) );
  if( ! block_solver_config )
   throw( std::logic_error( "BlockSolver group was not properly provided" ) );
  block_solver_config->apply( sddp_block );
  block_solver_config->clear();

  // Set the output stream for the log of the inner Solvers
  set_log( sddp_block , &std::cout );

  // Load possibly given cuts
  if( ! simulation_mode )
   sddp_block->deserialize_cuts( cuts_filename );

  // Eliminate redundant cuts if it is desired
  if( eliminate_redundant_cuts )
   CutProcessing( get_blocksolverconfig(
                              get_cut_processing_solver_config_filepath() )
                  ).remove_redundant_cuts( sddp_block );

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

BlockSolverConfig * build_BlockSolverConfig( void )
{
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

BlockConfig * build_BlockConfig( const SDDPBlock * sddp_block )
{
 /* The per-class configuration of the inner Block of each stage (including
  * all PolyhedralFunctionBlock) is dealt with by configure_Blocks(); here
  * we only build the structural part, i.e., the ComputeConfig of each
  * BendersBFunction, whose inner Block gets the default BlockSolverConfig
  * read from BendersBSCfg.txt. */

 auto benders_solver_config =
  get_blocksolverconfig( benders_solver_config_filename );

 if( ! benders_solver_config )
  std::cout << "Warning: default BlockSolverConfig file "
            << benders_solver_config_filename << " not found; the inner "
            << "Block of each BendersBFunction gets no Solver" << std::endl;

 auto sddp_config = new RBlockConfig;
 auto num_stochastic_blocks = sddp_block->get_number_nested_Blocks();

 for( Block::Index index = 0 ; index < num_stochastic_blocks ; ++index ) {
  auto benders_function_config = new ComputeConfig;
  benders_function_config->f_extra_Configuration =
   new SimpleConfiguration< std::map< std::string , Configuration * > >(
              { { "BlockConfig" , nullptr } ,
              { "BlockSolverConfig" , benders_solver_config ?
                benders_solver_config->clone() : nullptr } } );

  auto stochastic_block_config = new RBlockConfig;
  sddp_config->add_sub_BlockConfig( stochastic_block_config , index );

  auto benders_block_config = new OBlockConfig;
  stochastic_block_config->add_sub_BlockConfig( benders_block_config , 0 );

  benders_block_config->set_Config_Objective( benders_function_config );
  }

 delete benders_solver_config;

 return( sddp_config );
 }

/*--------------------------------------------------------------------------*/

bool using_lagrangian_dual_solver( BlockSolverConfig * sddp_solver_config )
{
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

  // note: no conf_prefix here: file names referenced inside configuration
  // files are relative to the directory the tool is run from, since they
  // are also opened by the loading Solver, which knows no prefix
  std::ifstream inner_solver_config_file( strInnerBSC , std::ifstream::in );
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
                             SDDPBlock * sddp_block )
{
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
  // note: no conf_prefix here, see the comment in the other overload
  std::ifstream inner_solver_config_file( strInnerBSC , std::ifstream::in );

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

 /* The BlockSolverConfig for the inner Block of each LagBFunction is given
  * by the (possibly "meta", i.e., dispatched by inner Block classname())
  * str_LagBF_BSCfg of the LagrangianDualSolver ComputeConfig; see
  * InnerBSCfg.txt and InnerBSCfg-sim.txt. Here we only decide which
  * components are hard (vintNoEasy) and whose primal solution is required.
  *
  * The vector "required_primal_solution" will store the indices of Blocks
  * whose primal solutions are required (during the solution process). In
  * SDDP, only the primal solution of the HydroSystemUnitBlock is necessary
  * (as only the final volumes of the reservoirs are required during the
  * solution process). In simulation mode, the primal solutions that are
  * required are those of the Blocks that link two consecutive stages, which
  * are HydroSystemUnitBlock, BatteryUnitBlock, and ThermalUnitBlock.
  *
  * Notice that, in simulation mode, not all Blocks have their primal
  * solutions retrieved, which impacts the part of the solution that is output
  * (see SDDPBlockSolution). If the solutions of other Blocks are required
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

   // The primal solution of the BatteryUnitBlock is required as the storage
   // levels link two consecutive stages. Since BundleSolver currently does
   // not provide primal solutions for easy components, the BatteryUnitBlock
   // must be treated as a hard component. Once this feature is implemented by
   // BundleSolver, the BatteryUnitBlock can become an easy component.
   required_primal_solution.push_back( inner_sub_block_index );
   vintNoEasy.push_back( inner_sub_block_index );
  }
  else if( dynamic_cast< ThermalUnitBlock * >( inner_sub_block ) ) {

   if( simulation_mode )
    required_primal_solution.push_back( inner_sub_block_index );

   // ThermalUnitBlock is a non-easy component since there is a specialized
   // solver for it.
   vintNoEasy.push_back( inner_sub_block_index );
  }
  else if( dynamic_cast< HydroSystemUnitBlock * >( inner_sub_block ) ) {
   required_primal_solution.push_back( inner_sub_block_index );
   hydro_system_index = inner_sub_block_index;

   /* The HydroSystemUnitBlock is by default treated as a hard component:
    * its primal solution (the volume of the reservoirs) is required both in
    * SDDP and in simulation mode, and BundleSolver does not provide primal
    * solutions of easy components out of the inner Solver. With the -z
    * option it is instead treated as an easy component, and its primal
    * solution is recovered from the master problem solution by passing the
    * Configuration in get_var_solution_bundle.txt to get_var_solution() of
    * the [Parallel]BundleSolver. */
   if( hydro_is_easy_component ) {
    lagrangian_dual_compute_config->vstr_pars.push_back(
     std::make_pair( "vstr_LDSl_Cfg" , std::vector< std::string >{
                     get_var_solution_bundle_filename } ) );
    lagrangian_dual_compute_config->int_pars.push_back(
     std::make_pair( "int_InnerS_WVarSCfg" , 0 ) );
    }
   else
    vintNoEasy.push_back( inner_sub_block_index );
  }
  else if( ( simulation_mode || force_hard_components ) &&
           dynamic_cast< IntermittentUnitBlock * >( inner_sub_block ) ) {

   if( simulation_mode )
    required_primal_solution.push_back( inner_sub_block_index );

   vintNoEasy.push_back( inner_sub_block_index );
  }
  else if( ( simulation_mode || force_hard_components ) &&
           dynamic_cast< NetworkBlock * >( inner_sub_block ) ) {
   // The dual solution of the NetworkBlock is part of the required output of
   // the simulation. Since BundleSolver currently does not provide solutions
   // for easy components, the NetworkBlock must be treated as a hard
   // component. Once this feature is implemented by BundleSolver, the
   // NetworkBlock can become an easy component.
   vintNoEasy.push_back( inner_sub_block_index );

   if( simulation_mode )
    required_primal_solution.push_back( inner_sub_block_index );
  }
  else if( ! do_easy_components ) {
   if( simulation_mode )
    required_primal_solution.push_back( inner_sub_block_index );

   vintNoEasy.push_back( inner_sub_block_index );
  }

  ++inner_sub_block_index;
 }

 if( ! vintNoEasy.empty() ) {
  // Remove any vintNoEasy parameter that is possibly there
  lagrangian_dual_compute_config->vint_pars.erase(
     std::remove_if( lagrangian_dual_compute_config->vint_pars.begin() ,
                     lagrangian_dual_compute_config->vint_pars.end() ,
                     []( const auto & pair ) {
                      return( pair.first == "vintNoEasy" ); } ) ,
     lagrangian_dual_compute_config->vint_pars.end() );

  // Add the vintNoEasy parameter that was constructed here
  lagrangian_dual_compute_config->vint_pars.push_back(
               std::make_pair( "vintNoEasy" , std::move( vintNoEasy ) ) );
  }

 // Configuration for the sub-Blocks may need to be cloned since the same
 // Configuration is used to configure multiple Blocks.
 lagrangian_dual_compute_config->int_pars.push_back(
                             std::make_pair( "int_LDSlv_CloneCfg" , 1 ) );

 erase_str_par( compute_config , "strInnerBSC" );

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

 get_var_solution_config = new SimpleConfiguration< std::vector< int > >(
                                                  required_primal_solution );

 if( simulation_mode ) {
  /* In simulation mode, the only part of the dual Solution that is required
   * is that associated with the linking constraints (the set of Constraint
   * defined in the UCBlock). Therefore, we create a Configuration for the
   * get_dual_solution() method that ignores the dual solutions of the
   * sub-Blocks of the UCBlock and requires the dual solutions of the linking
   * constraints. */

  get_dual_solution_config =
   new SimpleConfiguration< std::vector< std::pair< int , int > > >(
                               { std::make_pair< int , int >( -1 , -1 ) } );

  // Create the extra Configuration for SDDPGreedySolver.

  extra_config = new SimpleConfiguration< std::vector< Configuration * > >(
                 { nullptr , inner_solver_config , get_var_solution_config ,
                   get_dual_solution_config } );
  }
 else // Create the extra Configuration for SDDPSolver.
  extra_config = new SimpleConfiguration< std::vector< Configuration * > >(
               { nullptr , inner_solver_config , get_var_solution_config } );

 compute_config->f_extra_Configuration = extra_config;

 if( ( ! simulation_mode ) && ( hydro_system_index >= 0 ) ) {
  // Configure all BendersBFunction to retrieve the right portion of the dual
  // variables.

  // In SDDP, only the dual variables of the component defined by the
  // HydroSystemUnitBlock are needed, as all constraints handled by the
  // BendersBFunction belong to it.
  auto get_dual_config =
   new SimpleConfiguration< std::vector< std::pair< int ,
                                                    Configuration * > > >(
                       { std::make_pair( hydro_system_index , nullptr ) } );

  auto benders_function_config = new ComputeConfig;

  // Differential mode to keep the previous configuration.
  benders_function_config->set_diff( true );

  benders_function_config->f_extra_Configuration =
   new SimpleConfiguration< std::map< std::string , Configuration * > >
   ( { { "get_dual" , get_dual_config  } ,
       { "get_dual_partial" , get_dual_config->clone() } } );

  for( auto sub_block : sddp_block->get_nested_Blocks() ) {
   auto stochastic_block = static_cast< StochasticBlock * >( sub_block );
   auto benders_block = static_cast< BendersBlock * >(
                           stochastic_block-> get_nested_Blocks().front() );
   auto objective = static_cast< FRealObjective * >(
                                           benders_block->get_objective() );
   auto benders_function = static_cast< BendersBFunction * >(
                                                objective->get_function() );
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

void process_block_file( const netCDF::NcFile & file )
{
 std::multimap< std::string , netCDF::NcGroup > blocks = file.getGroups();

 // BlockConfig
 auto given_block_config = get_blockconfig( bconf_file );

 BlockConfig * block_config = nullptr;
 if( given_block_config ) {
  block_config = given_block_config->clone();
  block_config->clear();
  }

 // BlockSolverConfig
 bool block_solver_config_provided = true;
 auto solver_config = get_blocksolverconfig( sconf_file );
 if( ! solver_config ) {
  block_solver_config_provided = false;
  solver_config = build_BlockSolverConfig();
  }

 auto cleared_solver_config = solver_config->clone();
 cleared_solver_config->clear();

 const auto is_using_lagrangian_dual_solver =
  using_lagrangian_dual_solver( solver_config );

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
   configure_Blocks( sddp_block , is_using_lagrangian_dual_solver );

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
   sddp_block->deserialize_cuts( cuts_filename );

  // Eliminate redundant cuts if it is desired
  if( eliminate_redundant_cuts )
   CutProcessing( get_blocksolverconfig(
                              get_cut_processing_solver_config_filepath() )
                  ).remove_redundant_cuts( sddp_block );

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

std::vector< double > get_final_state( SDDPBlock * block , Index stage )
{
 Index state_size = 0;
 for( Index i = 0 ; i < block->get_num_polyhedral_function_per_sub_block() ; ++i )
  state_size += block->get_polyhedral_function( stage , i )->get_num_active_var();
 std::vector< double > state;
 state.reserve( state_size );

 for( Index i = 0 ; i < block->get_num_polyhedral_function_per_sub_block() ; ++i ) {
  const auto polyhedral_function = block->get_polyhedral_function( stage , i );
  for( const auto & variable : * polyhedral_function )
   state.push_back( static_cast< const ColVariable & >( variable ).get_value() );
  }

 return( state );
 }

/*--------------------------------------------------------------------------*/

void multiple_simulations( const netCDF::NcFile & file )
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
 bool block_solver_config_provided = true;
 auto solver_config = get_blocksolverconfig( sconf_file );
 if( ! solver_config ) {
  block_solver_config_provided = false;
  solver_config = build_BlockSolverConfig();
  }

 auto cleared_solver_config = solver_config->clone();
 cleared_solver_config->clear();

 const auto is_using_lagrangian_dual_solver =
  using_lagrangian_dual_solver( solver_config );

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
    configure_Blocks( sddp_block , is_using_lagrangian_dual_solver );

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
   auto solver = dynamic_cast< SDDPGreedySolver * >(
                             sddp_block->get_registered_solvers().front() );
   if( ! solver )
    throw( std::logic_error( "The Solver for the SDDPBlock must be a "
                             "SDDPGreedySolver in simulation mode." ) );

   if( solver->get_int_par( solver->int_par_str2idx( "intLogVerb" ) ) )
    solver->set_log( & std::cout );

   auto subgradients_filename_prefix =
    solver->get_str_par( SDDPGreedySolver::strSimulationData );

   // the (index of the) sub-Block of each stage the simulation works on
   const auto sub_block_index = Index( solver->get_int_par(
                                       SDDPGreedySolver::intSubBlockIndex ) );

   solver->set_callback( [ sddp_block , sub_block_index ]( Index stage ) {
                          callback( sddp_block , stage , sub_block_index );
                          } );

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
    CutProcessing( get_blocksolverconfig(
                               get_cut_processing_solver_config_filepath() )
                   ).remove_redundant_cuts( sddp_block );

   // Set the name of the file that will output the subgradients
   if( ! subgradients_filename_prefix.empty()  )
    solver->set_par( SDDPGreedySolver::strSimulationData ,
                     subgradients_filename_prefix + "." + std::to_string( i )
                     );

   // Try to solve the SDDPBlock
   while( true ) {
    const auto status = solver->compute();  // Simulate

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
     show_sddp_greedy_status( status , solver->get_fault_stage() );
     const auto lb = solver->get_lb();
     const auto ub = solver->get_ub();
     std::cout << "Lower bound: " << std::setprecision( 20 ) << lb
               << std::endl;
     std::cout << "Upper bound: " << std::setprecision( 20 ) << ub
               << std::endl;
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

void check_consistency( void )
{
 if( ! std::filesystem::is_directory( output_solution_directory ) ) {
  std::cerr << "Directory " << output_solution_directory
            << " does not exist" << std::endl;
  exit( 1 );
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );
 
 // append new options to default ones- - - - - - - - - - - - - - - - - - - -
 // note that the local options are inserted right before the last (nullptr)
 // record in long_opts
 
 docopt_desc = "SMS++ SDDP solver";

 // the Configuration files of this tool live in config/ by default; an
 // explicit -c overrides this
 conf_prefix = "config/";

 short_opts.append( my_short_opts );
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_long_opts.begin() , my_long_opts.end() );
 help.append( my_help );

 // process command-line arguments- - - - - - - - - - - - - - - - - - - - - -

 process_args( argc , argv , process_specific_arg );

 check_consistency();

 // open the file - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 netCDF::NcFile file;
 auto type = read_open_netCDF( file , filename );

 // process the file- - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 #ifdef USE_MPI
  // note: the MPI/UCX/hwloc safe-environment defaults are pre-seeded by
  // the SmsppMpiSafeEnvInit static initializer in common_utils.cpp
  boost::mpi::environment env( argc , argv );
 #endif

 switch( type ) {
  case eProbFile: std::cout << filename << " is a problem file, "
                            << "ignoring Block/Solver Configuration(s)..."
                            << std::endl;
                  process_prob_file( file );
                  break;

  case eBlockFile: std::cout << filename << " is a block file" << std::endl;
                   if( simulation_mode && ( number_simulations > 1 ) )
                    multiple_simulations( file );
                   else
                    process_block_file( file );
                   break;
  default: std::cerr << filename << " is not a valid SMS++ file" << std::endl;
           exit( 1 );
  }

 return( 0 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File sddp_solver.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
