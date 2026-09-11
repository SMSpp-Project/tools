/*-------------------------- File sfdcrblock_solver.cpp --------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * SMS++ SingleFlowDCRBlock solver.
 *
 * A tool that loads a SingleFlowDCRBlock from a file, configures it with a
 * BlockConfig and a BlockSolverConfig and solves it with all the loaded
 * Solvers; see solve_Block_file() in common_utils.h.
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "common_utils.h"
#include "SingleFlowDCRBlock.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 docopt_desc =
  "SMS++ SingleFlowDCRBlock solver: loads a Single-flow Delay-Constrained\n"
  "Routing problem (a SingleFlowDCRBlock) and solves it with the Solvers of\n"
  "its BlockSolverConfig.\n";
 docopt_args =
  "  <file>    SMS++ netCDF file (.nc4) holding a SingleFlowDCRBlock: a\n"
  "            Block file, or a problem file of which only the Block is\n"
  "            used; the DIMACS format does not carry the delay data\n";
 docopt_examples =
  "  sfdcrblock_solver instance.nc4\n"
  "      solve with the default configuration, i.e., with the Benders\n"
  "      Solver of SingleFlowDCRBlock\n"
  "  sfdcrblock_solver -B PCCfg.txt -S MILPCfg.txt instance.nc4\n"
  "      solve the P/C formulation with HiGHS, which, the cones being\n"
  "      outer-approximated by linear cuts, gives a lower bound\n"
  "  sfdcrblock_solver -c myconfig/ instance.nc4\n"
  "      use the Configuration files in myconfig/, e.g. a modified copy\n"
  "      of the installed ones\n";

 process_args( argc , argv );

 return( solve_Block_file< SingleFlowDCRBlock >(
          "SingleFlowDCRBlock" , false ) );
 }

/*--------------------------------------------------------------------------*/
/*---------------------- End File sfdcrblock_solver.cpp --------------------*/
/*--------------------------------------------------------------------------*/
