/*--------------------------------------------------------------------------*/
/*--------------------------- block_solver.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * SMS++ generic block and problem solver.
 *
 * A tool that loads an SMS++ nc4 Block or Problem file and solves it.
 *
 * In the case of a Block file, i.e., a file that contains one or more Blocks,
 * it optionally configures all the Blocks with a BlockConfig and/or a
 * BlockSolverConfig, then it solves it with all the loaded solvers.
 *
 * In the case of a Problem file, i.e., one that contains one or more Problems
 * (with a problem being a Block/BlockConfig/BlockSolverConfig tuple),
 * it solves each problem with all the loaded solvers.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Niccolo' Iardella \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni, Niccolo' Iardella
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <iostream>
#include <iomanip>

#include <Block.h>
#include <BlockSolverConfig.h>

#include "common_utils.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc, char ** argv )
{
 // manage options and help, see common_utils.h- - - - - - - - - - - - - - - -
 docopt_desc = "SMS++ generic Block solver";
 process_args( argc , argv );

 // read nc4 file- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 netCDF::NcFile f;
 auto type = read_open_netCDF( f , filename );

 if( type == eProbFile )
  // problem file containing one or more Block/BlockConfig/BlockSolver sets
  std::cout << filename
	    << " is a problem file, ignoring Block/Solver configurations"
	    << std::endl;

 // for each sub-group - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 auto groups = f.getGroups();

 for( auto & g : groups ) {
  Block * block = nullptr;
  BlockConfig * b_config = nullptr;
  BlockSolverConfig * s_config = nullptr;

  if( type == eProbFile ) {
   std::cout << "Problem: " << g.first << std::endl;
   get_all( g.second , block , b_config , s_config );
   }
  else {
   std::cout << "Block: " << g.first << std::endl;
   get_all( g.second , bconf_file , sconf_file ,
	    block , b_config , s_config );
   }

  solve_all( block );
  if( s_config )
   s_config->apply( block );

  delete s_config;
  delete b_config;
  delete block;

  }  // end( for( all group ) )

 return( 0 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------- end block_solver.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/

