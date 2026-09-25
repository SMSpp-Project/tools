/*--------------------------------------------------------------------------*/
/*------------------------ File investmentblock_solver.cpp -----------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * This is a convenient tool for solving the investment problem defined by an
 * InvestmentBlock. The description of the InvestmentBlock must be given in a
 * netCDF file. This tool can be executed as follows:
 *
 *   ./investmentblock_solver [-B FILE] [-p PATH] [-c PATH] [-x FILE ]
 *                            -S FILE <nc4-file>
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
 * For a BlockFile, the -S option specifies the BlockSolverConfig of every
 * InvestmentBlock, and the -B option its BlockConfig; for a ProbFile, both
 * come from the file, and -B only concerns the inner Block described below.
 * The -B file is typically a "meta"-BlockConfig, i.e., a
 *
 *   SimpleConfiguration< std::map< std::string , Configuration * > >
 *
 * mapping a Block classname() to the BlockConfig to be applied to every
 * Block of that class (see config/InnerBCfg.txt), which is dispatched to the
 * InvestmentBlock and inside the inner Block of its InvestmentFunction, the
 * UCBlock of each stage of an SDDPBlock included, since the nested-Block BFS
 * cannot cross a Function: it selects the formulation of, e.g., the
 * ThermalUnitBlock or the DCNetworkBlock of the inner UCBlock.
 *
 * The BlockConfig of the InvestmentBlock is an OBlockConfig (see
 * config/IBOCfg.txt): besides reformulating the bounds on the investment,
 * which BundleSolver needs to be of the form 0 <= x <= u, it gives the
 * InvestmentFunction its ComputeConfig (see config/IFCfg.txt), whose extra
 * Configuration holds the BlockSolverConfig of the inner Block (a UCBlock, a
 * TwoStageStochasticBlock or an SDDPBlock). Everything is thus said by the
 * configuration files, and the tool only reads and applies them.
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

std::string initial_point_filename {};

// State to be loaded into the InvestmentBlock Solver
std::string solver_state_input_filename{};

// Prefix to the name of the file that will store the State of the
// InvestmentBlock Solver
std::string solver_state_output_filename{};

std::vector< double > initial_point;

/*--------------------------------------------------------------------------*/

const std::string my_short_opts = "x:";

const std::vector< option > my_long_opts = {
  { "initial-investment" ,       required_argument , nullptr , 'x' }
  };

const std::string my_help =
 "  -x, --initial-investment <file> initial investment\n";

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 switch( opt ) {  // non-standard options
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

bool update_thermal_unit( Block * previous_block , Block * block ,
                          Index stage )
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

  // the simulation passes to the next stage the volumes of the reservoirs,
  // and the state of the thermal and battery units as well
  if( ! update_hydro_unit( previous_block , block , stage ) )
   update_thermal_unit( previous_block , block , stage )
    || update_battery_unit( previous_block , block , stage );
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

  // the bounds are reformulated (or not) when the constraints are generated
  investment_block->generate_abstract_constraints();
  if( investment_block->get_reformulate_bounds() ) {
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

void invest( InvestmentBlock * investment_block )
{
 auto investment_function = static_cast< InvestmentFunction * >(
					 investment_block->get_function() );

 // Optimize
 auto investment_solver = investment_block->get_registered_solvers().front();

 if( sol_verbose )
  investment_solver->set_log( &std::cout );

 // set initial Solution, if provided - - - - - - - - - - - - - - - - - - - -
 get_initial_Solution( investment_block );

 // load the given State, if provided - - - - - - - - - - - - - - - - - - - -
 get_initial_State( investment_solver );

 print_solver_parameters( { investment_block , investment_function } );

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
    if( investment_block->get_reformulate_bounds() && ( i < var_lb.size() ) &&
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
/// configures the inner Block of the InvestmentFunction
/** The inner Block of the InvestmentFunction, and the UCBlock of every stage
 * of an SDDPBlock, which sits behind a BendersBFunction, are out of reach of
 * the nested-Block BFS of config_Block() started at the InvestmentBlock: the
 * given "meta"-BlockConfig is dispatched to each of them here. */

void configure_inner_Blocks( InvestmentFunction * investment_function ,
                             Configuration * block_config )
{
 for( auto block : investment_function->get_nested_Blocks() )
  if( auto sddp_block = dynamic_cast< SDDPBlock * >( block ) ) {
   for( Index t = 0 ; t < sddp_block->get_time_horizon() ; ++t )
    for( Index j = 0 ; j < sddp_block->get_num_sub_blocks_per_stage() ; ++j )
     config_Block( get_uc_block( sddp_block , t , j ) , block_config ,
                   nullptr );
   }
  else
   config_Block( block , block_config , nullptr );
 }

/*--------------------------------------------------------------------------*/
/// checks that the InvestmentFunction has been given its Solver
/** The Solver of the inner Block of the InvestmentFunction come with the
 * ComputeConfig of the InvestmentFunction, which the OBlockConfig of the
 * InvestmentBlock gives to its Objective (see config/IBOCfg.txt). When the
 * inner Block is an SDDPBlock, the SDDPGreedySolver so registered, which
 * simulate the scenarios one stage after the other, are each given the
 * callback() that passes the final state of a stage to the next. */

void check_inner_Solvers( InvestmentFunction * investment_function )
{
 for( auto block : investment_function->get_nested_Blocks() ) {
  if( block->get_registered_solvers().empty() ) {
   std::cerr << "The inner Block of the InvestmentFunction has no Solver: "
             << "the BlockConfig of the InvestmentBlock must be an "
             << "OBlockConfig giving its InvestmentFunction a ComputeConfig "
             << "with a BlockSolverConfig (see config/IBOCfg.txt)."
             << std::endl;
   exit( 1 );
   }

  if( auto sddp_block = dynamic_cast< SDDPBlock * >( block ) )
   for( auto solver : sddp_block->get_registered_solvers() )
    if( auto greedy = dynamic_cast< SDDPGreedySolver * >( solver ) ) {
     const auto sub_block_index = Index( greedy->get_int_par(
                                     SDDPGreedySolver::intSubBlockIndex ) );
     greedy->set_callback( [ sddp_block , sub_block_index ]( Index stage ) {
                            callback( sddp_block , stage , sub_block_index );
                            } );
     }
  }
 }

/*--------------------------------------------------------------------------*/

void process_prob_file( const netCDF::NcFile & file )
{
 // the inner Block, which the BlockConfig of the problem does not reach, takes
 // the -B "meta"-BlockConfig, if any
 auto inner_block_config = get_config( bconf_file );

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

  auto investment_block = dynamic_cast< InvestmentBlock * >(
				 Block::new_Block( block_group , nullptr ) );
  assert( investment_block );

  auto investment_function = static_cast< InvestmentFunction * >(
					 investment_block->get_function() );

  // Configure the inner Block, then the InvestmentBlock, whose BlockConfig
  // gives the InvestmentFunction the Solver of the inner Block
  if( inner_block_config )
   configure_inner_Blocks( investment_function , inner_block_config );

  auto block_config_group = problem_group.getGroup( "BlockConfig" );
  auto block_config = dynamic_cast< BlockConfig * >(
		      BlockConfig::new_Configuration( block_config_group ) );
  if( ! block_config )
   throw( std::logic_error( "BlockConfig group was not properly provided" ) );
  // the OBlockConfig gives its ComputeConfig to the Objective, which has to
  // be there already; the constraints wait for the BlockConfig, which says
  // whether the bounds are reformulated
  investment_block->generate_abstract_variables();
  investment_block->generate_objective();
  block_config->apply( investment_block );
  block_config->clear();

  check_inner_Solvers( investment_function );

  // Possibly set the initial point
  set_initial_point( investment_block );

  // Configure solver
  auto solver_config_group = problem_group.getGroup( "BlockSolver" );
  auto block_solver_config = dynamic_cast< BlockSolverConfig * >(
	       BlockSolverConfig::new_Configuration( solver_config_group ) );
  if( ! block_solver_config )
   throw( std::logic_error( "BlockSolver group was not properly provided" ) );
  block_solver_config->apply( investment_block );
  block_solver_config->clear();

  std::cout << "Problem: " << problem.first << std::endl;

  // Set the output stream for the log of the inner Solvers

  for( auto block : investment_function->get_nested_Blocks() )
   if( auto sddp_block = dynamic_cast< SDDPBlock * >( block ) )
    set_log( sddp_block , &std::cout );
   else
    for( auto solver : block->get_registered_solvers() )
     if( solver )
      solver->set_log( &std::cout );

  // Solve
  invest( investment_block );

  // Destroy the Block and the Configurations

  block_config->apply( investment_block );
  delete block_config;

  block_solver_config->apply( investment_block );
  delete block_solver_config;

  delete investment_block;
  }

 delete inner_block_config;
 }

/*--------------------------------------------------------------------------*/

void process_block_file( const netCDF::NcFile & file )
{
 auto blocks = file.getGroups();

 // [meta]BlockConfig
 auto given_block_config = get_config( bconf_file );
 if( ! given_block_config ) {
  std::cerr << "A BlockConfig for the InvestmentBlock must be given (-B), "
            << "which is an OBlockConfig or a \"meta\"-BlockConfig with one "
            << "(see config/InnerBCfg.txt)." << std::endl;
  exit( 1 );
  }
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
  // investment is the here-and-now decision taken above the scenarios, or
  // an SDDPBlock, in which case it is taken above the stages
  for( auto block_ : investment_function->get_nested_Blocks() )
   if( ! ( dynamic_cast< UCBlock * >( block_ ) ||
           dynamic_cast< TwoStageStochasticBlock * >( block_ ) ||
           dynamic_cast< SDDPBlock * >( block_ ) ) ) {
    std::cerr << "The sub-Block of the InvestmentBlock is neither a UCBlock "
              << "nor a TwoStageStochasticBlock nor an SDDPBlock."
              << std::endl;
    exit( 1 );
    }

  // Configure the inner Block, then the InvestmentBlock, whose (O)BlockConfig
  // gives the InvestmentFunction the Solver of the inner Block: a plain
  // BlockConfig only concerns the InvestmentBlock, while a "meta"-BlockConfig
  // is also dispatched inside the InvestmentFunction, since the nested-Block
  // BFS of config_Block() cannot cross the Function boundary
  if( ! given_plain_block_config )
   configure_inner_Blocks( investment_function , given_block_config );
  // the OBlockConfig gives its ComputeConfig to the Objective, which has to
  // be there already; the constraints wait for the BlockConfig, which says
  // whether the bounds are reformulated
  investment_block->generate_abstract_variables();
  investment_block->generate_objective();
  config_Block( investment_block , given_block_config , nullptr );

  check_inner_Solvers( investment_function );

  // Possibly set the initial point

  set_initial_point( investment_block );

  // Finally, apply the Solver configuration

  solver_config->apply( investment_block );

  // Set the output stream for the log of the inner Solvers

  for( auto block : investment_function->get_nested_Blocks() )
   if( auto sddp_block = dynamic_cast< SDDPBlock * >( block ) )
    set_log( sddp_block , &std::cout );
   else
    for( auto solver : block->get_registered_solvers() )
     if( solver )
      solver->set_log( &std::cout );

  // Solve
  invest( investment_block );

  // Destroy the InvestmentBlock and the Configurations

  if( block_config )
   block_config->apply( investment_block );

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
  "  investmentblock_solver -c myconfig/ instance.nc4\n"
  "      use the Configuration files in myconfig/, e.g. a modified copy\n"
  "      of the installed ones\n";
 short_opts.append( my_short_opts );
 long_opts.insert( std::prev( long_opts.end() ) ,
		   my_long_opts.begin() , my_long_opts.end() );
 help.append( my_help );

 // the InvestmentBlock is solved by a BundleSolver, configured in BSPar.txt;
 // InnerBCfg.txt shapes the inner Block of the InvestmentFunction and, by the
 // OBlockConfig of the InvestmentBlock, gives the InvestmentFunction the
 // BlockSolverConfig of the inner Block (BSCfg.txt)
 default_bconf_name = "InnerBCfg.txt";
 default_sconf_name = "BSPar.txt";

 // process command-line arguments- - - - - - - - - - - - - - - - - - - - - -

 process_args( argc , argv , process_specific_arg );

 // open the file - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // the BlockSolverConfig (-S) is mandatory, and so is the BlockConfig (-B)
 // for a Block file, which can default to the conventional files (see the
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
/*-------------------- End File investmentblock_solver.cpp -----------------*/
/*--------------------------------------------------------------------------*/
