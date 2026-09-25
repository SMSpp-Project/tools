/*--------------------------------------------------------------------------*/
/*---------------------------- sddp_config.h -------------------------------*/
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

#ifndef __SDDP_CONFIG
 #define __SDDP_CONFIG

#include <string>

#include <BlockSolverConfig.h>
#include <SDDPBlock.h>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/
/// whether the stages are solved by a LagrangianDualSolver
/** Returns true if the BlockSolverConfig named by the strInnerBSC parameter
 * of one of the [Parallel]SDDPSolver or SDDPGreedySolver in the given
 * BlockSolverConfig (of an SDDPBlock) has a LagrangianDualSolver. */

bool using_lagrangian_dual_solver( BlockSolverConfig * sddp_solver_config );

/*--------------------------------------------------------------------------*/
/// configures the LagrangianDualSolver that solves the stages of an SDDPBlock
/** If the BlockSolverConfig named by the strInnerBSC parameter of a
 * [Parallel]SDDPSolver or SDDPGreedySolver in sddp_solver_config has a
 * LagrangianDualSolver whose inner Solver is a [Parallel]BundleSolver, the
 * ComputeConfig of that Solver gets, as its "extra" Configuration, that
 * BlockSolverConfig, with the components that must be hard (vintNoEasy),
 * and the Configuration saying whose primal solutions are needed; unless the
 * Solver is an SDDPGreedySolver, the BendersBFunction of each stage of
 * sddp_block are also told to retrieve the dual solution of the
 * HydroSystemUnitBlock alone. The structure of the stages is read from those
 * of sddp_block.
 *
 * The primal solution of the HydroSystemUnitBlock is always needed, as the
 * volume of its reservoirs links a stage to the next. If simulation_mode is
 * true, the states of the ThermalUnitBlock and of the BatteryUnitBlock have
 * to pass from a stage to the next as well, which requires their primal
 * solutions and makes them hard components; so it is for the
 * IntermittentUnitBlock and the NetworkBlock, whose solutions are output, and
 * for them also if force_hard_components is true. If hydro_is_easy_component
 * is true the HydroSystemUnitBlock is left easy, and its primal solution is
 * recovered from the master problem with the Configuration in the file
 * get_var_solution_bundle_filename. */

void config_Lagrangian_dual( BlockSolverConfig * sddp_solver_config ,
                             SDDPBlock * sddp_block , bool simulation_mode ,
                             bool hydro_is_easy_component ,
                             bool force_hard_components ,
                             const std::string &
                             get_var_solution_bundle_filename );

/*--------------------------------------------------------------------------*/

#endif  /* sddp_config.h included */

/*--------------------------------------------------------------------------*/
/*------------------------ End File sddp_config.h --------------------------*/
/*--------------------------------------------------------------------------*/
