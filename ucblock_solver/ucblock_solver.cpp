/** @file
 * SMS++ unit commitment solver.
 *
 * A tool that loads an UCBlock from a SMS++ nc4 Block file,
 * optionally configures it with a BlockConfig and a BlockSolverConfig,
 * and solves it with all the loaded Solvers.
 *
 * Optionally, it writes back the Block, the BlockConfig and the
 * BlockSolverConfig on a SMS++ nc4 problem file.
 *
 * \author Niccolo' Iardella \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Niccolo' Iardella
 */

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <iostream>
#include <iomanip>

#include <Block.h>
#include <BlockSolverConfig.h>

#include "common_utils.h"
#include "ucblock_utils.h"

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc, char ** argv ) {

 // Manage options and help, see common_utils.h
 docopt_desc = "SMS++ unit commitment solver.\n";
 process_args( argc, argv );

 // Deserialize block
 Block * block = Block::deserialize( filename );
 if( ! block ) {
  std::cerr << exe << ": "
            << "Block::deserialize() failed!" << std::endl;
  exit( 1 );
 }

 // Configure block
 BlockConfig * b_config;
 if( ! bconf_file.empty() ) {
  b_config = get_blockconfig( bconf_file );
  if( b_config == nullptr ) {
   std::cerr << exe << ": Block configuration not valid" << std::endl;
   exit( 1 );
  }
 } else {
  // TODO: Try to remove this
  std::cout << "Using a default Block configuration" << std::endl;
  b_config = default_configure_UCBlock( block );
 }
 b_config->apply( block );

 // Configure solver
 BlockSolverConfig * s_config;
 if( ! sconf_file.empty() ) {
  s_config = get_blocksolverconfig( sconf_file );
  if( s_config == nullptr ) {
   std::cerr << exe << ": Solver configuration not valid" << std::endl;
   exit( 1 );
  }
 } else {
  std::cout << "Using a default Solver configuration" << std::endl;
  s_config = default_configure_solver( solvVerbose );
 }
 s_config->apply( block );

 // Write nc4 problem
 if( writeprob )
  write_nc4problem( block, b_config, s_config );

 // Solve
 std::cout.setf( std::ios::scientific, std::ios::floatfield );
 std::cout << std::setprecision( 8 );
 solve_all( block );

 // Print the results
 print_UCBlock_solver_results( block , solution_output_type );

 return( 0 );
}  // end( main )

/*--------------------------------------------------------------------------*/
/*---------------------- End File ucblock_solver.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
