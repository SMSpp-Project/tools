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
 * BlockFile. The -B option specifies either a BlockConfig, applied to every
 * SDDPBlock, or a "meta" BlockConfig (a map from Block classname() to the
 * BlockConfig to be applied to every Block of that class, with "*" as the
 * default), dispatched to every Block of the inner Block of each stage; the
 * default is SDDPBCfg.txt. The -S option specifies the BlockSolverConfig of
 * every SDDPBlock, SDDPSCfg.txt by default. The inner Blocks are solved by a
 * LagrangianDualSolver with -B SDDPBCfg-LD.txt -S SDDPSCfg-LD.txt, and
 * -S SDDPSCfg-greedy-LD.txt in simulation mode.
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
#include <ThermalUnitBlock.h>
#include <PolyhedralFunctionBlock.h>
#include <SDDPBlock.h>
#include <StochasticBlock.h>
#include <SDDPGreedySolver.h>
#include <SDDPSolver.h>
#include <UCBlock.h>


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

// If set_polyhedral_function_bound is true, the PolyhedralFunction of each
// stage (the Bellman, cost-to-go function) gets the finite global lower
// bound below: since the stage costs are non-negative, 0 is a valid bound,
// and a bounded PolyhedralFunction keeps the stage subproblems bounded even
// when no (initial) cut is available
constexpr bool set_polyhedral_function_bound = true;
constexpr double polyhedral_function_bound = 0;

/*--------------------------------------------------------------------------*/

const std::string my_short_opts = "d:e:l:n:i:m:st:";

const std::vector< option > my_long_opts = {
  { "output-dir" ,               required_argument , nullptr , 'd' } ,
  { "eliminate-redundant-cuts" , required_argument , nullptr , 'e' } ,
  { "load-cuts" ,                required_argument , nullptr , 'l' } ,
  { "num-blocks" ,               required_argument , nullptr , 'n' } ,
  { "scenario" ,                 required_argument , nullptr , 'i' } ,
  { "num-simulations" ,          required_argument , nullptr , 'm' } ,
  { "simulation" ,               no_argument ,       nullptr , 's' } ,
  { "stage" ,                    required_argument , nullptr , 't' }
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
 "  -t, --stage <stage>             stage from which initial state is taken";

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
  case 's': simulation_mode = true; return( true );
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
// eliminate the redundant cuts of every PolyhedralFunction of the SDDPBlock,
// if requested: the removal is delegated to the PolyhedralFunctionBlock that
// owns each function ( reached via its Observer )

void process_cuts( SDDPBlock * sddp_block )
{
 if( ! eliminate_redundant_cuts )
  return;

 auto solver_config = get_blocksolverconfig(
			      get_cut_processing_solver_config_filepath() );

 for( auto function : sddp_block->get_polyhedral_functions() )
  if( auto pfb = dynamic_cast< PolyhedralFunctionBlock * >(
					       function->get_Observer() ) )
   pfb->remove_redundant_rows( solver_config );

 if( solver_config ) {
  solver_config->clear();
  delete( solver_config );
  }
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
 process_cuts( sddp_block );

 solver->set_scenario_id( scenario_id );

 // load the given State (the cuts), if provided - - - - - - - - - - - - - - -
 get_initial_State( solver );

 print_solver_parameters( sddp_block );

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

 print_solver_parameters( sddp_block );

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

 process_cuts( sddp_block );

 serialize_cuts( sddp_block , "BellmanValuesOUT.nc4" );
 serialize_cuts( sddp_block , "BellmanValuesOUT.csv" );

 }  // end( solve )

/*--------------------------------------------------------------------------*/

void configure_Blocks( SDDPBlock * sddp_block , Configuration * block_config )
{
 if( ! block_config )
  return;

 // an ordinary BlockConfig is applied to the SDDPBlock
 if( ! dynamic_cast< SimpleConfiguration< std::map< std::string ,
                                                    Configuration * > > * >(
                                                             block_config ) ) {
  config_Block( sddp_block , block_config , nullptr );
  return;
  }

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

 // the "meta" BlockConfig is dispatched to the Blocks of the inner Block
 // of each stage, which sits behind the BendersBFunction
 for( Index stage = 0 ; stage < sddp_block->get_time_horizon() ; ++stage )
  for( Index j = 0 ; j < sddp_block->get_num_sub_blocks_per_stage() ; ++j )
   config_Block( get_uc_block( sddp_block , stage , j ) , block_config ,
                 nullptr );
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
  process_cuts( sddp_block );

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

void process_block_file( const netCDF::NcFile & file )
{
 std::multimap< std::string , netCDF::NcGroup > blocks = file.getGroups();

 // [meta]BlockConfig; an ordinary one is cleared at the end
 auto block_config = get_config( bconf_file );

 BlockConfig * cleared_block_config = nullptr;
 if( auto bc = dynamic_cast< BlockConfig * >( block_config ) ) {
  cleared_block_config = bc->clone();
  cleared_block_config->clear();
  }

 // BlockSolverConfig
 auto solver_config = get_blocksolverconfig( sconf_file );
 if( ! solver_config ) {
  std::cerr << "no BlockSolverConfig for the SDDPBlock (-S)" << std::endl;
  exit( 1 );
  }

 auto cleared_solver_config = solver_config->clone();
 cleared_solver_config->clear();

 // For each Block descriptor
 for( auto block_description : blocks ) {

  // Deserialize the SDDPBlock
  auto sddp_block = new SDDPBlock;
  sddp_block->set_num_sub_blocks_per_stage( num_sub_blocks_per_stage );
  sddp_block->deserialize( block_description.second );

  // Configure the SDDPBlock
  configure_Blocks( sddp_block , block_config );

  // Configure the Solver
  solver_config->apply( sddp_block );

  // Set the output stream for the log of the inner Solvers
  set_log( sddp_block , &std::cout );

  // Load possibly given cuts
  if( ! simulation_mode )
   sddp_block->deserialize_cuts( cuts_filename );

  // Eliminate redundant cuts if it is desired
  process_cuts( sddp_block );

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

  if( cleared_block_config )
   cleared_block_config->apply( sddp_block );

  cleared_solver_config->apply( sddp_block );

  delete( sddp_block );
  }

 delete( block_config );
 delete( cleared_block_config );
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

 // [meta]BlockConfig; an ordinary one is cleared at the end
 auto block_config = get_config( bconf_file );

 BlockConfig * cleared_block_config = nullptr;
 if( auto bc = dynamic_cast< BlockConfig * >( block_config ) ) {
  cleared_block_config = bc->clone();
  cleared_block_config->clear();
  }

 // BlockSolverConfig
 auto solver_config = get_blocksolverconfig( sconf_file );
 if( ! solver_config ) {
  std::cerr << "no BlockSolverConfig for the SDDPBlock (-S)" << std::endl;
  exit( 1 );
  }

 auto cleared_solver_config = solver_config->clone();
 cleared_solver_config->clear();

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
   configure_Blocks( sddp_block , block_config );

   // Configure the Solver
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
   process_cuts( sddp_block );

   // Set the name of the file that will output the subgradients
   if( ! subgradients_filename_prefix.empty()  )
    solver->set_par( SDDPGreedySolver::strSimulationData ,
                     subgradients_filename_prefix + "." + std::to_string( i )
                     );

   // printed for the first simulation only, the others differ from it just
   // in the initial state and in the file of the subgradients
   if( i == 0 )
    print_solver_parameters( sddp_block );

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
   if( cleared_block_config )
    cleared_block_config->apply( sddp_block );

   cleared_solver_config->apply( sddp_block );
   delete( sddp_block );
   }
  }

 delete( block_config );
 delete( cleared_block_config );
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
 
 docopt_desc =
  "SMS++ SDDP solver: loads a multi-stage stochastic problem (an SDDPBlock)\n"
  "and solves it by Stochastic Dual Dynamic Programming, or simulates the\n"
  "policy of given cuts.\n";
 docopt_args =
  "  <file>    SMS++ netCDF file (.nc4) holding an SDDPBlock: a Block file,\n"
  "            or a problem file, whose own configuration is then used and\n"
  "            -B and -S are ignored; <file> and the files of the stages it\n"
  "            refers to are looked up under the -p prefix\n";
 docopt_examples =
  "  sddp_solver -p dir/ SDDPBlock.nc4\n"
  "      solve dir/SDDPBlock.nc4, whose stages are in dir/ too, with the\n"
  "      default configuration\n"
  "  sddp_solver -d out/ -p dir/ SDDPBlock.nc4\n"
  "      the same, writing the Bellman values and the solutions in out/,\n"
  "      which must exist\n"
  "  sddp_solver -c myconfig/ -p dir/ SDDPBlock.nc4\n"
  "      use the Configuration files in myconfig/, e.g. a modified copy\n"
  "      of the installed ones\n";

 // the Configuration files of this tool live in config/ by default; an
 // explicit -c overrides this
 conf_prefix = "config/";

 // -n is the number of sub-Blocks per stage here, not the nc4 problem
 drop_standard_option( 'n' );
 short_opts.append( my_short_opts );
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_long_opts.begin() , my_long_opts.end() );
 help.append( my_help );

 // default -B / -S so a plain run needs neither: the "meta" BlockConfig
 // SDDPBCfg.txt, which linearises the PolyhedralFunctionBlock and shapes the
 // network/units (a generic BCfg.txt would leave the PolyhedralFunctionBlock
 // objective non-linear, and MILPSolver throws "Unknown type of Objective
 // Function"), and the SDDPSolver BlockSolverConfig SDDPSCfg.txt
 default_bconf_name = "SDDPBCfg.txt";
 default_sconf_name = "SDDPSCfg.txt";

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
