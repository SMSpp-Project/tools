/*-------------------------- File mcfblock_solver.cpp ----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * SMS++ MCFBlock solver.
 *
 * A tool that loads a MCFBlock from a file, configures it with a
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
#include "MCFBlock.h"

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
  "SMS++ MCFBlock solver: loads a single-commodity Min-Cost Flow problem\n"
  "(an MCFBlock) and solves it with the Solvers of its BlockSolverConfig.\n";
 docopt_args =
  "  <file>    the MCFBlock, in an SMS++ netCDF file (.nc4, a Block file,\n"
  "            or a problem file of which only the Block is used) or in the\n"
  "            DIMACS format of MCFBlock::load()\n";
 docopt_examples =
  "  mcfblock_solver instance.dmx\n"
  "      solve with the default configuration, i.e., with MCFSimplex\n"
  "  mcfblock_solver -O sol.nc4 instance.nc4\n"
  "      also save the flows in sol.nc4\n"
  "  mcfblock_solver -c myconfig/ instance.dmx\n"
  "      use the Configuration files in myconfig/, e.g. a modified copy\n"
  "      of the installed ones\n";

 process_args( argc , argv );

 return( solve_Block_file< MCFBlock >( "MCFBlock" ) );
 }

/*--------------------------------------------------------------------------*/
/*---------------------- End File mcfblock_solver.cpp ----------------------*/
/*--------------------------------------------------------------------------*/
