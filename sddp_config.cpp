/*--------------------------------------------------------------------------*/
/*--------------------------- sddp_config.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * The configuration of the Solver of the stages of an SDDPBlock, shared by
 * the tools that solve one: sddp_solver, which solves the SDDPBlock itself,
 * and investmentblock_solver, which solves an InvestmentBlock whose inner
 * Block is an SDDPBlock.
 *
 * When the UCBlock of each stage is solved by a LagrangianDualSolver whose
 * inner Solver is a [Parallel]BundleSolver, the tool has to tell it which
 * components are hard and whose primal solutions are needed, and the
 * BendersBFunction of each stage which part of the dual solution it has to
 * retrieve: config_Lagrangian_dual() does that, and
 * using_lagrangian_dual_solver() says whether it is the case.
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

#include "sddp_config.h"
#include "common_utils.h"

#include <algorithm>
#include <fstream>

#include <BatteryUnitBlock.h>
#include <BendersBFunction.h>
#include <BendersBlock.h>
#include <FRealObjective.h>
#include <HydroSystemUnitBlock.h>
#include <IntermittentUnitBlock.h>
#include <NetworkBlock.h>
#include <StochasticBlock.h>
#include <ThermalUnitBlock.h>

/*--------------------------------------------------------------------------*/
/*------------------------------- FUNCTIONS --------------------------------*/
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

  // resolve strInnerBSC against the executable-wide Configuration prefix (the
  // -c prefix, see set_filename_prefix() in process_args), exactly as the
  // loading Solver does when it re-opens this same file via
  // Configuration::deserialize()
  std::ifstream inner_solver_config_file(
   resolve_with_prefix( Configuration::get_filename_prefix() , strInnerBSC ) ,
   std::ifstream::in );
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
                             SDDPBlock * sddp_block , bool simulation_mode ,
                             bool hydro_is_easy_component ,
                             bool force_hard_components ,
                             const std::string &
                             get_var_solution_bundle_filename )
{
 if( sddp_block->get_number_nested_Blocks() == 0 )
  // The SDDPBlock has no sub-Block. There is nothing to be configured.
  return;

 BlockSolverConfig * inner_solver_config = nullptr;
 ComputeConfig * lagrangian_dual_compute_config = nullptr;
 ComputeConfig * compute_config = nullptr;

 // whether the ComputeConfig is that of an SDDPGreedySolver, which only
 // simulates, rather than of a Solver that trains the cuts
 bool greedy = false;

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
  greedy = ( sddp_solver_config->get_SolverName( i ) == "SDDPGreedySolver" );

  // Check if strInnerBSC is present
  auto strInnerBSC = get_str_par( compute_config , "strInnerBSC" );
  if( strInnerBSC.empty() )
   return;

  // If it is, check if it is a config for a LagrangianDualSolver
  // resolve strInnerBSC against the Configuration prefix, see the other overload
  std::ifstream inner_solver_config_file(
   resolve_with_prefix( Configuration::get_filename_prefix() , strInnerBSC ) ,
   std::ifstream::in );

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

 if( greedy ) {
  /* The SDDPGreedySolver only simulates, and the only part of the dual
   * Solution that it requires is that associated with the linking
   * constraints (the set of Constraint
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

 if( ( ! greedy ) && ( hydro_system_index >= 0 ) ) {
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
/*----------------------- End File sddp_config.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
