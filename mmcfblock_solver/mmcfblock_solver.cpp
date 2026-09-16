/*-------------------------- File mmcfblock_solver.cpp ---------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * SMS++ MMCFBlock solver.
 *
 * A tool that loads a MMCFBlock from a file, configures it with a
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
#include "MMCFBlock.h"

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
  "SMS++ MMCFBlock solver: loads a Multicommodity Min-Cost Flow problem (an\n"
  "MMCFBlock) and solves it with the Solvers of its BlockSolverConfig.\n";
 docopt_args =
  "  <file>    the MMCFBlock, in an SMS++ netCDF file (.nc4, a Block file,\n"
  "            or a problem file of which only the Block is used) or in a\n"
  "            text format of MMCFBlock::load(), see -f\n";
 docopt_examples =
  "  mmcfblock_solver instance.dat\n"
  "      solve an instance of the Canad set with the default\n"
  "      configuration, i.e., the MILP with HiGHS\n"
  "  mmcfblock_solver -f m 64-4-1\n"
  "      the same, for the Mnetgen instance in 64-4-1.nod, .arc, .mut\n"
  "      and .sup\n"
  "  mmcfblock_solver -c myconfig/ instance.dat\n"
  "      use the Configuration files in myconfig/, e.g. a modified copy\n"
  "      of the installed ones\n";
 add_format_option(
  "                                  s = Canad [default], c = PPRN,\n"
  "                                  m = Mnetgen (<file>.nod, .arc, .mut and\n"
  "                                  .sup), p, o, d, u = Jones-Lustig PSP,\n"
  "                                  OSP, ODS and ODS with <file>.od\n"
  );
 // the default one of the formats of the input file
 input_format = 's';

 process_args( argc , argv );

 // the sub-Blocks of an MMCFBlock are created with its abstract Variable,
 // which is therefore done before any Solver locks it
 return( solve_Block_file< MMCFBlock >( "MMCFBlock" , true ,
          []( MMCFBlock * b ) { b->generate_abstract_variables(); } ) );
 }

/*--------------------------------------------------------------------------*/
/*---------------------- End File mmcfblock_solver.cpp ---------------------*/
/*--------------------------------------------------------------------------*/
