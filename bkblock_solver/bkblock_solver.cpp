/*-------------------------- File bkblock_solver.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * SMS++ BinaryKnapsackBlock solver.
 *
 * A tool that loads a BinaryKnapsackBlock from a file, configures it with a
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
#include "BinaryKnapsackBlock.h"

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
  "SMS++ BinaryKnapsackBlock solver: loads a Binary Knapsack problem (a\n"
  "BinaryKnapsackBlock) and solves it with the Solvers of its\n"
  "BlockSolverConfig.\n";
 docopt_args =
  "  <file>    the BinaryKnapsackBlock, in an SMS++ netCDF file (.nc4, a\n"
  "            Block file, or a problem file of which only the Block is\n"
  "            used) or in a text format of BinaryKnapsackBlock::load(),\n"
  "            see -f\n";
 docopt_examples =
  "  bkblock_solver instance.txt\n"
  "      solve with the default configuration, i.e., with the core DP\n"
  "  bkblock_solver -f P knapPI_1_100_1000_1\n"
  "      the same, for an instance of the Pisinger benchmarks\n"
  "  bkblock_solver -c myconfig/ instance.txt\n"
  "      use the Configuration files in myconfig/, e.g. a modified copy\n"
  "      of the installed ones\n";
 add_format_option(
  "                                  0 = the native one [default],\n"
  "                                  P = the Pisinger / Jooken benchmarks\n"
  );

 process_args( argc , argv );

 return( solve_Block_file< BinaryKnapsackBlock >( "BinaryKnapsackBlock" ) );
 }

/*--------------------------------------------------------------------------*/
/*---------------------- End File bkblock_solver.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
