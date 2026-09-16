/*-------------------------- File cflblock_solver.cpp ----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * SMS++ CapacitatedFacilityLocationBlock solver.
 *
 * A tool that loads a CapacitatedFacilityLocationBlock from a file,
 * configures it with a BlockConfig and a BlockSolverConfig and solves it
 * with all the loaded Solvers; see solve_Block_file() in common_utils.h.
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
#include "CapacitatedFacilityLocationBlock.h"

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
  "SMS++ CapacitatedFacilityLocationBlock solver: loads a Capacitated\n"
  "Facility Location problem (a CapacitatedFacilityLocationBlock) and\n"
  "solves it with the Solvers of its BlockSolverConfig.\n";
 docopt_args =
  "  <file>    the CapacitatedFacilityLocationBlock, in an SMS++ netCDF\n"
  "            file (.nc4, a Block file, or a problem file of which only\n"
  "            the Block is used) or in a text format of its load(), see -f\n";
 docopt_examples =
  "  cflblock_solver capa.txt\n"
  "      solve with the default configuration, i.e., the MILP with HiGHS\n"
  "  cflblock_solver -O sol.nc4 capa.txt\n"
  "      also save the facilities and the allocations in sol.nc4\n"
  "  cflblock_solver -c myconfig/ capa.txt\n"
  "      use the Configuration files in myconfig/, e.g. a modified copy\n"
  "      of the installed ones\n";
 add_format_option(
  "                                  C = ORLib [default], F = facility\n"
  "                                  oriented with the demands first,\n"
  "                                  L = the same with the demands last\n"
  );

 process_args( argc , argv );

 return( solve_Block_file< CapacitatedFacilityLocationBlock >(
          "CapacitatedFacilityLocationBlock" ) );
 }

/*--------------------------------------------------------------------------*/
/*---------------------- End File cflblock_solver.cpp ----------------------*/
/*--------------------------------------------------------------------------*/
