/*--------------------------------------------------------------------------*/
/*-------------------------- File ucblock_solver.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * SMS++ unit commitment solver.
 *
 * A tool that loads a UCBlock from an SMS++ nc4 Block file,
 * optionally configures it with a BlockConfig and a BlockSolverConfig,
 * and solves it with all the loaded Solvers.
 *
 * Optionally, it writes back the Block, the BlockConfig and the
 * BlockSolverConfig on an SMS++ nc4 problem file.
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
#include <exception>
#include <iomanip>

#include <Block.h>

#include "common_utils.h"
#include "UCBlock.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

// const std::string my_short_opts = "";

// const std::vector< option > my_long_opts = {};

// const std::string my_help = "";

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 // ucblock_solver has no tool-specific options
 return( false );
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // append new options to default ones- - - - - - - - - - - - - - - - - - - -
 // note that the local options are inserted right before the last (nullptr)
 // record in long_opts

 docopt_desc = "SMS++ UCBlock solver\n";
 /*short_opts.append( my_short_opts );
 long_opts.insert( std::prev( long_opts.end() ) ,
		   my_long_opts.begin() , my_long_opts.end() );
 help.append( my_help );*/

 // process command-line arguments- - - - - - - - - - - - - - - - - - - - - -

 process_args( argc , argv , process_specific_arg );

 // deserialize UCBlock - - - - - - - - - - - - - - - - - - - - - - - - - - -
 Block * block = get_Block( filename );

 if( ! dynamic_cast< UCBlock * >( block ) ) {
  std::cerr << exe << ": " << filename << " is not a UCBlock" << std::endl;
  exit( 1 );
  }

 // BlockConfig-ure and BlockSolverConfig-ure the [UC]Block - - - - - - - - -
 // if no BlockConfig file is given the UCBlock is solved as deserialized:
 // any non-default formulation choice is the user's job (-B)
 Configuration * b_config = nullptr;
 if( ! bconf_file.empty() )
  b_config = get_config( bconf_file );

 auto s_config = get_config( sconf_file );

 config_Block( block , b_config , s_config );

 // write nc4 problem - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 if( writeprob )
  write_nc4problem( block , b_config , s_config );

 // solve - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 std::cout.setf( std::ios::scientific, std::ios::floatfield );
 std::cout << std::setprecision( 8 );
 solve_all( block );

 // cleanup - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 cleanup_bsc( block , s_config );
 delete s_config;
 delete block;

 return( 0 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*---------------------- End File ucblock_solver.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
