/*--------------------------------------------------------------------------*/
/*------------------------------ File test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * This is a convenient tool for solving the investment problem defined by an
 * InvestmentBlock. The description of the InvestmentBlock must be given in a
 * netCDF file. This tool can be executed as follows:
 *
 *   ./test [-s] [-r] [-B FILE] [-p PATH] [-c PATH] [-x FILE ]
 *          -S FILE <nc4-file>
 *
 * The only mandatory arguments are the netCDF file containing the description
 * of the InvestmentBlock and the solver configuration file indicated by the
 * -S option. This netCDF file can be either a BlockFile or a ProbFile. The
 * BlockFile can contain any number of child groups, each one describing an
 * InvestmentBlock. Every InvestmentBlock is then solved. The ProbFile can
 * also contain any number of child groups, each one having the description of
 * an InvestmentBlock alongside the description of a BlockConfig and a
 * BlockSolverConfig for the InvestmentBlock. Also in this case, every
 * InvestmentBlock is solved.
 *
 * The -c option specifies the prefix to the paths to all configuration
 * files. This means that if PATH is the value passed to the -c option, then
 * the name (or path) to each configuration file will be prepended by
 * PATH. The -p option specifies the prefix to the paths to all files
 * specified by the attribute "filename" in the input netCDF file.
 *
 * It is possible to provide an initial point (initial solution or initial
 * investment) through the -x option. This option must be followed by a file
 * containing the initial point. If there are N assets subject to investment,
 * then this file must contain N numbers, where the i-th number is the initial
 * value for the investment in the i-th asset. If this option is not used,
 * then the initial value x_i for the investment in the i-th asset is
 * determined as follows. If the lower bound l_i on the i-th investment is
 * finite, then x_i = l_i. Otherwise, if the upper bound u_i on the i-th
 * investment is finite, then x_i = u_i. Otherwise, if both bounds are not
 * finite, then x_i = 0.
 *
 * The -r option indicates that the integrality constraints over the variables
 * must be relaxed.
 *
 * To simulate a given investment, i.e., to compute the investment function at
 * a given point, the -s option must be used. The investment to be simulated
 * is given by the initial point as described above: a given point provided by
 * the -x option or the default initial point.
 *
 * The -B and -S options are only considered if the given netCDF file is a
 * BlockFile. The -B option specifies a BlockConfig file to be applied to
 * every InvestmentBlock; while the -S option specifies a BlockSolverConfig
 * file for every InvestmentBlock. If the -B option is not provided when the
 * given netCDF file is a BlockFile, then a default configuration is
 * considered. The -B file can also contain a "meta"-BlockConfig, i.e., a
 *
 *   SimpleConfiguration< std::map< std::string , Configuration * > >
 *
 * mapping a Block classname() to the BlockConfig to be applied to every
 * Block of that class (see config/InnerBCfg.txt); it is dispatched both
 * to the InvestmentBlock and inside the inner Block of its
 * InvestmentFunction, so it can be used to select the formulation of, e.g.,
 * the ThermalUnitBlock or the DCNetworkBlock of the inner UCBlock.
 *
 * The BlockSolverConfig for the inner Block of the InvestmentFunction (the
 * UCBlock) is indicated by the strInnerBSC string parameter in the
 * ComputeConfig of the Solver of the InvestmentBlock found in the -S file
 * (see config/BSPar.txt).
 *
 * \author Rafael Durbano Lobato \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Rafael Durbano Lobato, Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "common_utils.h"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <queue>

#include <BatteryUnitBlock.h>
#include <BendersBlock.h>
#include <BlockSolverConfig.h>
#include <HydroSystemUnitBlock.h>
#include <IntermittentUnitBlock.h>
#include <NetworkBlock.h>
#include <SDDPBlock.h>
#include <StochasticBlock.h>
#include <SDDPGreedySolver.h>
#include <SDDPSolver.h>
#include <SlackUnitBlock.h>
#include <ThermalUnitBlock.h>
#include <TwoStageStochasticBlock.h>

#include <UCBlock.h>

#include "InvestmentBlock.h"
#include "InvestmentFunction.h"

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

std::string cuts_filename {};
std::string initial_point_filename {};

// State to be loaded into the InvestmentBlock Solver
std::string solver_state_input_filename{};

// Prefix to the name of the file that will store the State of the
// InvestmentBlock Solver
std::string solver_state_output_filename{};

long num_sub_blocks_per_stage = 1;

bool relax_integrality = false;
bool simulate_investment = false;
bool single_scenario = false;

// Since BundleSolver cannot currently handle general bounds on the variables
// of the form l <= x <= u, these constraints must be reformulated by
// replacing them by 0 <= x <= u - l.
const bool reformulate_variable_bounds = true;

// This variable indicates whether negative prices may occur
const bool negative_prices = false;

// It indicates whether the investment function is based on simulation only
// (true) or SDDP (false).
bool simulation_based_function = true;

std::vector< double > initial_point;

/*--------------------------------------------------------------------------*/

const std::string my_short_opts = "l:n:rsx:";

const std::vector< option > my_long_opts = {
  { "load-cuts" ,                required_argument , nullptr , 'l' } ,
  { "num-blocks" ,               required_argument , nullptr , 'n' } ,
  { "relax" ,                    no_argument ,       nullptr , 'r' } ,
  { "simulate" ,                 no_argument ,       nullptr , 's' } ,
  { "initial-investment" ,       required_argument , nullptr , 'x' }
  };

const std::string my_help =
 "  -l, --load-cuts <file>          load cuts from a file\n"
 "  -n, --num-blocks <number>       number of sub-Blocks per stage\n"
 "  -r, --relax                     relax integer variables\n"
 "  -s, --simulate                  simulate the given investment\n"
 "  -x, --initial-investment <file> initial investment\n";

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 switch( opt ) {  // non-standard options
  case 'l': cuts_filename = std::string( optarg ); return( true );
  case 'n': num_sub_blocks_per_stage = get_long_option();
            if( num_sub_blocks_per_stage <= 0 ) {
             std::cout << "The number of sub-Blocks per stage must be a "
                       << "positive integer." << std::endl;
             exit( 1 );
             }
            return( true );
  case 'r': relax_integrality = true; return( true );
  case 's': simulate_investment = true; return( true );
  case 'x': initial_point_filename = std::string( optarg ); return( true );
  case '?':
  default:  return( false );
  }
 } // end( process_specific_arg )

/*--------------------------------------------------------------------------*/

Block * get_uc_block( const SDDPBlock * sddp_block , Index stage ,
		      Index sub_block_index )
{
 auto benders_block = static_cast< BendersBlock * >(
   sddp_block->get_sub_Block( stage , sub_block_index )->get_inner_block() );

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
  throw( std::logic_error( "test: UCBlocks at stages " +
			   std::to_string( stage - 1 ) + " and " +
			   std::to_string( stage ) +
			   " do not have the same structure" ) );

 auto number_generators = previous_unit->get_number_generators();

 if( number_generators != unit->get_number_generators() )
  throw( std::logic_error( "test: HydroUnitBlock at stage " +
			   std::to_string( stage - 1 ) + " has " +
			   std::to_string( number_generators ) +
			   ", but corresponding HydroUnitBlock at stage " +
			   std::to_string( stage ) + " has " +
			   std::to_string( unit->get_number_generators() )
			   ) );

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
  throw( std::logic_error( "test: UCBlocks at stages " +
			   std::to_string( stage - 1 ) +
			   " and " + std::to_string( stage ) +
			   " do not have the same structure" ) );

 const auto time_horizon = previous_unit->get_time_horizon();

 std::vector< double > initial_power_data = {
  ( previous_unit->get_active_power( 0 ) + time_horizon - 1 )->get_value() };

 unit->set_initial_power( initial_power_data.cbegin() );

 std::vector< double > initial_storage_data = {
  previous_unit->get_storage_level()[ time_horizon - 1 ].get_value() };

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
 if( on ) init_up_down_time = 1;
 else     init_up_down_time = -1;

 AbstractPath path;

 for( Index outer_t = 0 ; outer_t < stage ; ++outer_t ) {
  for( Index t = 1 ; t < time_horizon ; ++t , --commitment ) {
   if( std::abs( commitment->get_value() -
                 ( commitment - 1 )->get_value() ) > 0.5 )
    return( init_up_down_time );
   if( on ) ++init_up_down_time;
   else     --init_up_down_time;
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

 if( ! unit && ! previous_unit )
  return( false );

 if( ! unit || ! previous_unit )
  throw( std::logic_error(
	   "test: UCBlocks at stages " + std::to_string( stage - 1 ) +
           " and " + std::to_string( stage ) +
           " do not have the same structure." ) );

 if( single_scenario ) {
  // The only way to update the initial up and down time is when there is a
  // single scenario.
  auto init_up_down_time = compute_init_up_down_time( sddp_block ,
			    previous_unit , unit , stage , sub_block_index );

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

void callback( SDDPBlock * sddp_block , Index stage , Index sub_block_index ) {
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
   throw( std::logic_error( "test: UCBlocks at stages " +
			    std::to_string( stage - 1 ) +
			    " and " + std::to_string( stage ) +
			    " do not have the same structure" ) );

  for( decltype( n ) i = 0 ; i < n ; ++i ) {
   blocks.push( block->get_nested_Block( i ) );
   previous_blocks.push( previous_block->get_nested_Block( i ) );
   }

  if( ( ! update_hydro_unit( previous_block , block , stage ) ) &&
      simulation_based_function ) {
   // In SDDP, only the reservoir volumes (of the hydro units) are transmitted
   // from one stage to the next. In simulation, on the other hand, data from
   // thermal and battery units are also passed from one stage to the
   // next. Thefore, initial states of thermal and battery units should only
   // be updated when the simulation-based function is considered.

   update_thermal_unit( sddp_block , previous_block , block , stage ,
                        sub_block_index )
    || update_battery_unit( previous_block , block , stage );
   }
  }
 }

/*--------------------------------------------------------------------------*/

std::vector< double > get_default_initial_point( InvestmentBlock * block )
{
 block->generate_abstract_constraints();
 const auto & box_constraints = block->get_constraints();
 std::vector< double > initial_point( box_constraints.size() );
 for( Index i = 0 ; i < box_constraints.size() ; ++i )
  if( box_constraints[ i ].get_lhs() > -Inf< double >() )
   initial_point[ i ] = box_constraints[ i ].get_lhs();
  else
   if( box_constraints[ i ].get_rhs() < Inf< double >() )
    initial_point[ i ] = box_constraints[ i ].get_rhs();
   else
    initial_point[ i ] = 0;

 return( initial_point );
 }

/*--------------------------------------------------------------------------*/

std::vector< double > load_initial_point( void )
{
 if( initial_point_filename.empty() )
  return {};

 std::ifstream file( initial_point_filename );

 // Make sure the file is open
 if( ! file.is_open() )
  throw( std::runtime_error( "It was not possible to open the file " +
                             initial_point_filename ) );

 std::vector< double > initial_point;

 double component;
 while( file >> component )
  initial_point.push_back( component );

 return( initial_point );
 }

/*--------------------------------------------------------------------------*/

void set_initial_point( InvestmentBlock * investment_block )
{
 // Generate the abstract variables so that we can set their values.

 investment_block->generate_abstract_variables();

 // Possibly load a given initial point.

 initial_point = load_initial_point();

 if( ! initial_point.empty() ) {
  // An initial point has been provided.

  const auto num_variables = investment_block->get_number_variables();
  if( initial_point.size() != num_variables )
   throw( std::logic_error( "The initial point has size " +
                            std::to_string( initial_point.size() ) + ", but "
                            "there are " + std::to_string( num_variables ) +
                            " variables." ) );

  if( reformulate_variable_bounds ) {
   // If variable bounds have been reformulated, the initial point must be
   // adjusted.

   const auto & var_lower_bound =
    investment_block->get_variable_lower_bound();
   for( Index i = 0 ; i < initial_point.size() ; ++i ) {
    if( ( i < var_lower_bound.size() ) &&
        ( var_lower_bound[ i ] > -Inf< double >() ) )
     initial_point[ i ] -= var_lower_bound[ i ];
    }
   }
  }
 else  // Since no initial point has been provided, we use the default one.
  initial_point = get_default_initial_point( investment_block );

 // Finally, set the initial point.
 investment_block->set_variable_values( initial_point );
 }

/*--------------------------------------------------------------------------*/

void load_cuts( SDDPBlock * sddp_block )
{
 if( cuts_filename.empty() )
  return;

 std::ifstream cuts_file( cuts_filename );

 // Make sure the file is open
 if( ! cuts_file.is_open() )
  throw( std::runtime_error( "It was not possible to open the file " +
                             cuts_filename ) );

 const auto time_horizon = sddp_block->get_time_horizon();

 std::vector< PolyhedralFunction::MultiVector > A( time_horizon ,
				        PolyhedralFunction::MultiVector {} );
 std::vector< PolyhedralFunction::RealVector > b( time_horizon ,
					PolyhedralFunction::RealVector {} );
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
   throw( std::logic_error( "File " + cuts_filename + "contains invalid"
                            " stage " + std::to_string( stage ) ) );

  if( line_stream.peek() != ',' )
   throw( std::logic_error( "File " + cuts_filename +
                            " has an invalid format." ) );
  line_stream.ignore();

  // Read the cut

  const auto polyhedral_function =
   sddp_block->get_polyhedral_function( stage );
  const auto num_active_var = polyhedral_function->get_num_active_var();
  PolyhedralFunction::RealVector a( num_active_var );

  Index i = 0;
  double value;
  while( line_stream >> value ) {
   if( i > num_active_var )
    throw( std::logic_error( "File " + cuts_filename + " contains an invalid"
			     " cut at line " + std::to_string( line_number )
			     ) );
   if( i < num_active_var )
    a[ i ] = value;
   else
    b[ stage ].push_back( value );

   ++i;

   if( line_stream.peek() == ',' )
    line_stream.ignore();
   }

  if( i < num_active_var )
   throw( std::logic_error( "File " + cuts_filename + " contains an invalid"
			    " cut at line " + std::to_string( line_number )
			    ) );
  A[ stage ].push_back( a );
  }

 cuts_file.close();

 // Now, add the cuts to all PolyhedralFunctions

 for( Index stage = 0 ; stage < time_horizon ; ++stage ) {
  for( Index sub_block_index = 0 ;
       sub_block_index < sddp_block->get_num_sub_blocks_per_stage() ;
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

void invest( InvestmentBlock * investment_block )
{
 auto investment_function = static_cast< InvestmentFunction * >(
					 investment_block->get_function() );

 // Optimize
 auto investment_solver = investment_block->get_registered_solvers().front();

 if( sol_verbose )
  investment_solver->set_log( &std::cout );

 // Output the variable and function values at each iteration
 investment_function->set_par( InvestmentFunction::strOutputFilename ,
                               "investment_candidates.txt" );

 // set initial Solution, if provided - - - - - - - - - - - - - - - - - - - -
 get_initial_Solution( investment_block );

 // load the given State, if provided - - - - - - - - - - - - - - - - - - - -
 get_initial_State( investment_solver );

 // solve the investment problem- - - - - - - - - - - - - - - - - - - - - - -
 if( ! dryrun ) {
  auto status = investment_solver->compute();
  std::cout << "Solver status: " << status << std::endl;
  }

 // write final Solution, if required - - - - - - - - - - - - - - - - - - - -
 write_final_Solution( investment_block );

 // write final State, if required- - - - - - - - - - - - - - - - - - - - - -
 write_final_State( investment_solver );

 if( output_solution ) {  // display the solution
  if( investment_solver->has_var_solution() ) {
   const auto solution_value = investment_solver->get_var_value();
   std::cout << "Solution value: " << std::setprecision( 20 )
	     << solution_value << std::endl;
   investment_solver->get_var_solution();
   std::cout << "Solution: " << std::endl;
   const auto & variables = investment_block->get_variables();
   const auto & var_lb = investment_block->get_variable_lower_bound();
   const auto width = std::to_string( variables.size() ).size();
   for( Index i = 0 ; i < variables.size() ; ++i ) {
    auto value = variables[ i ].get_value();
    if( reformulate_variable_bounds && ( i < var_lb.size() ) &&
	( var_lb[ i ] > -Inf< double >() ) )
     value += var_lb[ i ];
    std::cout << std::setw( width ) << i << " " << value << std::endl;
    }
   }
  else
  std::cout << "No solution has been found" << std::endl;
  }
 }

/*--------------------------------------------------------------------------*/

void configure_Blocks( UCBlock * ucblock , bool relax_binary_variables ,
                       bool add_reserve_variables_to_objective )
{
 std::queue< Block * > blocks;
 blocks.push( ucblock );

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
   config->f_static_variables_Configuration =
    new SimpleConfiguration< int >( 1 );
   polyhedral->set_BlockConfig( config );
   }

  else if( auto unit = dynamic_cast< SlackUnitBlock * >( block ) ) {
   auto config = new BlockConfig;
   /*
   config->f_static_variables_Configuration =
    new SimpleConfiguration< int >( var_type );
   */
   config->f_static_constraints_Configuration =
    new SimpleConfiguration< int >( cons_type );
   unit->set_BlockConfig( config );
   }
  else if( auto unit = dynamic_cast< BatteryUnitBlock * >( block ) ) {
   auto config = new BlockConfig;
   config->f_static_variables_Configuration = new SimpleConfiguration<
    std::pair< int , int > >( { negative_prices , var_type } );
   config->f_static_constraints_Configuration =
    new SimpleConfiguration< int >( cons_type );
   unit->set_BlockConfig( config );
   }
  else if( auto unit = dynamic_cast< ThermalUnitBlock * >( block ) ) {
   auto config = new BlockConfig;
   /*
   config->f_static_variables_Configuration =
    new SimpleConfiguration< int >( var_type );
   */
   config->f_static_constraints_Configuration =
    new SimpleConfiguration< int >( cons_type );

   /*
   if( add_reserve_variables_to_objective )
    config->f_objective_Configuration = new SimpleConfiguration< int >( 3 );
   */

   unit->set_BlockConfig( config );
   }
  }
 }

/*--------------------------------------------------------------------------*/

void set_log( SDDPBlock * sddp_block , std::ostream * output_stream )
{
 for( auto sub_block : sddp_block->get_nested_Blocks() ) {
  for( auto solver : sddp_block->get_registered_solvers() )
   if( solver )
    solver->set_log( output_stream );

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

 for( auto & problem : problems ) {  // for each problem descriptor:
  auto & problem_group = problem.second;

  // Deserialize the Block
  auto block_group = problem_group.getGroup( "Block" );
  auto block_type_att = block_group.getAtt( "type" );

  if( block_type_att.isNull() ) {
   std::cerr << "Attribute 'type' not found in the netCDF group "
             << block_group.getName() << std::endl;
   exit( 1 );
   }

  std::string block_type;
  block_type_att.getValues( block_type );

  if( block_type != "InvestmentBlock" ) {
   std::cerr << "The Block in the netCDF file " << block_type << " is "
             << block_type << ", but it must be an InvestmentBlock"
             << std::endl;
   exit( 1 );
   }

  std::function< void( Block * ) > set_num_sub_blocks( []( Block * block ) {
   if( auto investment_block = dynamic_cast< InvestmentBlock * >( block ) )
    investment_block->set_num_sub_blocks_per_stage( num_sub_blocks_per_stage
						    );
   else {
    std::cerr << "Error while deserializing the InvestmentBlock" << std::endl;
    exit( 1 );
    } } );

  auto investment_block = dynamic_cast< InvestmentBlock * >(
				 Block::new_Block( block_group , nullptr ) );
   // TODO
   //( Block::new_Block( block_group , nullptr , &set_num_sub_blocks ) );

  assert( investment_block );

  auto investment_function = static_cast< InvestmentFunction * >(
					 investment_block->get_function() );

  for( auto sddp_block_ : investment_function->get_nested_Blocks() ) {
   auto sddp_block = dynamic_cast< SDDPBlock * >( sddp_block_ );

   if( ! sddp_block ) {
    std::cerr << "The sub-Block of the InvestmentBlock is not an SDDPBlock"
              << std::endl;
    exit( 1 );
    }
   }

  // Configure block
  auto block_config_group = problem_group.getGroup( "BlockConfig" );
  auto block_config = static_cast< BlockConfig * >(
		      BlockConfig::new_Configuration( block_config_group ) );
  if( ! block_config )
   throw( std::logic_error( "BlockConfig group was not properly provided" ) );
  block_config->apply( investment_block );
  block_config->clear();

  // Possibly set the initial point
  set_initial_point( investment_block );

  // Configure solver
  auto solver_config_group = problem_group.getGroup( "BlockSolver" );
  auto block_solver_config = static_cast< BlockSolverConfig * >(
	       BlockSolverConfig::new_Configuration( solver_config_group ) );
  if( ! block_solver_config )
   throw( std::logic_error( "BlockSolver group was not properly provided" ) );
  block_solver_config->apply( investment_block );
  block_solver_config->clear();

  std::cout << "Problem: " << problem.first << std::endl;

  // Set the output stream for the log of the inner Solvers

  for( auto sddp_block_ : investment_function->get_nested_Blocks() ) {
   auto sddp_block = dynamic_cast< SDDPBlock * >( sddp_block_ );
   set_log( sddp_block , &std::cout );
   }

  // Solve
  invest( investment_block );

  // Destroy the Block and the Configurations

  block_config->apply( investment_block );
  delete block_config;

  block_solver_config->apply( investment_block );
  delete block_solver_config;

  delete investment_block;
  }
 }

/*--------------------------------------------------------------------------*/

void process_block_file( const netCDF::NcFile & file )
{
 auto blocks = file.getGroups();

 // [meta]BlockConfig
 auto given_block_config = get_config( bconf_file );
 auto given_plain_block_config =
  dynamic_cast< BlockConfig * >( given_block_config );

 BlockConfig * block_config = nullptr;
 if( given_plain_block_config ) {
  block_config = given_plain_block_config->clone();
  block_config->clear();
  }

 // BlockSolverConfig
 auto solver_config = get_blocksolverconfig( sconf_file );
 if( ! solver_config ) {
  std::cerr << "The Solver configuration is not valid." << std::endl;
  exit( 1 );
  }

 // the BlockSolverConfig for the inner Block of the InvestmentFunction is
 // indicated by the strInnerBSC parameter in the ComputeConfig of (one of)
 // the Solver of the InvestmentBlock; since it is not a real parameter of
 // that Solver, it is removed from the ComputeConfig before this is applied
 std::string inner_bsc_filename;
 for( Index i = 0 ; i < solver_config->num_ComputeConfig() ; ++i ) {
  auto compute_config = solver_config->get_SolverConfig( i );
  if( ! compute_config )
   continue;
  inner_bsc_filename = get_str_par( compute_config , "strInnerBSC" );
  if( inner_bsc_filename.empty() )
   continue;
  erase_str_par( compute_config , "strInnerBSC" );
  break;
  }

 // if strInnerBSC was not given, fall back to the conventional inner
 // BlockSolverConfig, but only when it is actually reachable, so a plain run
 // needs no explicit strInnerBSC and the parameter stays genuinely optional
 if( inner_bsc_filename.empty() )
  inner_bsc_filename = default_config_file( "BSCfg.txt" );

 if( inner_bsc_filename.empty() ) {
  std::cerr << "The BlockSolverConfig for the inner Block of the "
            << "InvestmentFunction must be given via the strInnerBSC "
            << "parameter in the ComputeConfig of the Solver of the "
            << "InvestmentBlock, or be reachable as the conventional "
            << "BSCfg.txt." << std::endl;
  exit( 1 );
  }

 report_config_file( "inner BlockSolverConfig (strInnerBSC)" ,
                     inner_bsc_filename );

 auto cleared_solver_config = solver_config->clone();
 cleared_solver_config->clear();

 // For each Block descriptor
 for( auto block_description : blocks ) {

  // Deserialize the Block
  auto block_type_att = block_description.second.getAtt( "type" );
  if( block_type_att.isNull() ) {
   std::cerr << "The netCDF attribute 'type' was not found in the netCDF "
             << "group " << block_description.second.getName() << std::endl;
   exit( 1 );
   }

  std::string block_type;
  block_type_att.getValues( block_type );

  if( block_type != "InvestmentBlock" ) {
   std::cerr << "The Block in the netCDF file " << block_type << " is "
             << block_type << ", but it must be an InvestmentBlock"
             << std::endl;
   exit( 1 );
   }

  auto investment_block = dynamic_cast< InvestmentBlock * >(
		  Block::new_Block( block_description.second , nullptr ) );
  assert( investment_block );

  auto investment_function = static_cast< InvestmentFunction * >(
				        investment_block->get_function() );

  // the inner Block may also be a stochastic one, in which case the
  // investment is the here-and-now decision taken above the scenarios
  for( auto block_ : investment_function->get_nested_Blocks() )
   if( ! ( dynamic_cast< UCBlock * >( block_ ) ||
           dynamic_cast< TwoStageStochasticBlock * >( block_ ) ) ) {
    std::cerr << "The sub-Block of the InvestmentBlock is neither a UCBlock "
              << "nor a TwoStageStochasticBlock." << std::endl;
    exit( 1 );
    }

  // Configure the Block
  if( given_block_config ) {
   // a plain BlockConfig is just apply()-ed to the InvestmentBlock, while a
   // "meta"-BlockConfig is also dispatched to the inner Block of the
   // InvestmentFunction, since the nested-Block BFS of config_Block()
   // cannot cross the Function boundary
   config_Block( investment_block , given_block_config , nullptr );
   if( ! given_plain_block_config )
    for( auto block_ : investment_function->get_nested_Blocks() )
     config_Block( block_ , given_block_config , nullptr );
   }
  else
   for( auto block_ : investment_function->get_nested_Blocks() ) {
    bool is_using_lagrangian_dual_solver = false;
    if( auto block = dynamic_cast< UCBlock * >( block_ ) )
     configure_Blocks( block , relax_integrality ,
                       is_using_lagrangian_dual_solver );
    else
     // a stochastic inner Block holds one UCBlock per scenario: each of them
     // is configured, the stochastic Block itself having nothing to configure
     for( auto scenario : block_->get_nested_Blocks() )
      if( auto block = dynamic_cast< UCBlock * >( scenario ) )
       configure_Blocks( block , relax_integrality ,
                         is_using_lagrangian_dual_solver );
    }

  if( reformulate_variable_bounds ) {
   // Since BundleSolver cannot currently handle general bounds on the
   // variables of the form l <= x <= u, we create a BlockConfig to instruct
   // the InvestmentBlock to reformulate the bound constraints by replacing
   // l <= x <= u by 0 <= x <= u - l.
   auto config = new BlockConfig;
   config->f_static_constraints_Configuration =
    new SimpleConfiguration< int >( 1 );

   investment_block->set_BlockConfig( config );
   }

  // Configure the Solver

  auto ucblock_solver_config = get_blocksolverconfig( inner_bsc_filename );

  if( ! ucblock_solver_config ) {
   std::cerr << "File " << inner_bsc_filename << " was not found or "
             << "its Configuration is invalid." << std::endl;
   exit( 1 );
   }

  // Construct the ComputeConfig for the InvestmentFunction

  ComputeConfig investment_function_config;

  investment_function_config.f_extra_Configuration =
   new SimpleConfiguration< std::map< std::string , Configuration * > >
   ( { { "BlockSolverConfig" , ucblock_solver_config  } } );

  investment_function->set_ComputeConfig( &investment_function_config );

  // Possibly set the initial point

  set_initial_point( investment_block );

  // Finally, apply the Solver configuration

  solver_config->apply( investment_block );

  // Set the output stream for the log of the inner Solvers

  for( auto block : investment_function->get_nested_Blocks() ) {
   for( auto solver : block->get_registered_solvers() )
    if( solver )
     solver->set_log( &std::cout );
   }

  // Solve
  invest( investment_block );

  // Destroy the InvestmentBlock and the Configurations

  if( block_config )
   block_config->apply( investment_block );
  if( ! given_block_config ) {
   delete block_config;
   block_config = nullptr;
   }

  cleared_solver_config->apply( investment_block );

  delete investment_block;
  }

 delete block_config;
 delete given_block_config;
 delete solver_config;
 delete cleared_solver_config;
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // append new options to default ones- - - - - - - - - - - - - - - - - - - -
 // note that the local options are inserted right before the last (nullptr)
 // record in long_opts

 #ifdef USE_MPI
  boost::mpi::environment env( argc , argv );
 #endif

 docopt_desc =
  "SMS++ investment solver: loads an investment problem (an InvestmentBlock)\n"
  "and solves it with the Solvers of its BlockSolverConfig, a BundleSolver\n"
  "on the investment decisions by default.\n";
 docopt_args =
  "  <file>    SMS++ netCDF file (.nc4) holding an InvestmentBlock: a Block\n"
  "            file, or a problem file, whose own configuration is then\n"
  "            used and -B and -S are ignored\n";
 docopt_examples =
  "  investmentblock_solver instance.nc4\n"
  "      solve with the default configuration\n"
  "  investmentblock_solver -r instance.nc4\n"
  "      the same, with the integer variables relaxed\n"
  "  investmentblock_solver -c myconfig/ instance.nc4\n"
  "      use the Configuration files in myconfig/, e.g. a modified copy\n"
  "      of the installed ones\n";
 // -n is the number of sub-Blocks per stage here, not the nc4 problem
 drop_standard_option( 'n' );
 short_opts.append( my_short_opts );
 long_opts.insert( std::prev( long_opts.end() ) ,
		   my_long_opts.begin() , my_long_opts.end() );
 help.append( my_help );

 // the InvestmentBlock is solved by a BundleSolver, configured in BSPar.txt;
 // the inner Block of the InvestmentFunction is shaped by InnerBCfg.txt and
 // solved as per BSCfg.txt, the conventional default for strInnerBSC (see
 // process_block_file())
 default_bconf_name = "InnerBCfg.txt";
 default_sconf_name = "BSPar.txt";

 // process command-line arguments- - - - - - - - - - - - - - - - - - - - - -

 process_args( argc , argv , process_specific_arg );

 // open the file - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // only the BlockSolverConfig (-S) is mandatory; the BlockConfig (-B) is
 // optional, defaulting to a built-in configuration when not given (see the
 // default file lookup in process_args())
 require_solver_config( sconf_file );

 netCDF::NcFile file;
 auto type = read_open_netCDF( file , filename );

// process the file- - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 switch( type ) {
  case eProbFile: std::cout << filename << " is a problem file, "
			    << "ignoring Block/Solver Configuration(s)..."
			    << std::endl;
                  process_prob_file( file );
		  break;
  case eBlockFile: std::cout << filename << " is a Block file" << std::endl;
                   process_block_file( file );
		   break;
  default: std::cerr << filename << " is not a valid SMS++ file"
		     << std::endl;
           exit( 1 );
  }

 return( 0 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
