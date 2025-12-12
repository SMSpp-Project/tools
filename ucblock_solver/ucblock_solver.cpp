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
#include <BlockSolverConfig.h>

#include "common_utils.h"
#include "ucblock_utils.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

int solution_output_type = 1;

/*--------------------------------------------------------------------------*/

const std::string my_short_opts = "t:";

const std::vector< option > my_long_opts = {
  { "output   " ,                required_argument , nullptr , 't' }
  };

const std::string my_help =
 "  -t, --output <type>             solution output type [1]\n"
 "                                  (0 none, 1 screen, 2 files, 3 both)\n";

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

void process_my_args( int argc , char ** argv )
{
 exe = get_filename( argv[ 0 ] );
 if( argc < 2 ) {
  std::cout << exe << ": no input file\n"
            << "Try " << exe << "' --help' for more information.\n";
  exit( 1 );
  }

 while( true ) { // options
  auto opt = getopt_long( argc , argv , short_opts.data() ,
                          long_opts.data() , nullptr );
  if( opt == -1 ) break;
  if( process_standard_arg( opt ) ) // if it is a standard one
   continue; // next

  switch( opt ) { // non-standard options
  case 't' : {
   auto s = std::string( optarg );
   if( ( s.size() != 1 ) || ( s.front() < '0' ) || ( s.front() > '3' ) ) {
    std::cout << "Invalid output solution type " << s << std::endl
     << "Try " << exe << "' --help' for more information"
     << std::endl;
    exit( 1 );
   }
   solution_output_type = s.front() - '0';
   break;
  }
  case '?' : // Unrecognized option
  default : std::cout << "Try " << exe << "' --help' for more information"
    << std::endl;
   exit( 1 );
  }
 } // end( while( true ) )

 if( optind < argc )  // last argument == [UCBlock] filename
  filename = std::string( argv[ optind ] );
 else {
 std::cout << exe << ": no input file" << std::endl
            << "Try " << exe << "' --help' for more information" << std::endl;
  exit( 1 );
  }
 } // end( process_my_args )

/*--------------------------------------------------------------------------*/

int main( int argc, char ** argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );
 
 // append new options to default ones- - - - - - - - - - - - - - - - - - - -
 // note that the local options are inserted right before the last (nullptr)
 // record in long_opts

 docopt_desc = "SMS++ UCBlock solver\n";
 short_opts.append( my_short_opts );
 long_opts.insert( std::prev( long_opts.end() ) ,
		   my_long_opts.begin() , my_long_opts.end() );
 help.append( my_help );

 // process command-line arguments- - - - - - - - - - - - - - - - - - - - - -

 process_my_args( argc , argv );

 // deserialize UCBlock - - - - - - - - - - - - - - - - - - - - - - - - - - -
 Block * block = Block::deserialize( filename );
 if( ! block ) { 
  std::cerr << exe << ": Block::deserialize() failed" << std::endl;
  exit( 1 );
  }

 if( ! dynamic_cast< UCBlock * >( block ) ) {
  std::cerr << exe << ": " << filename << " is not a UCBlock" << std::endl;
  exit( 1 );
  }

 // Configure Block - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 BlockConfig * b_config;
 if( ! bconf_file.empty() ) {
  b_config = get_blockconfig( bconf_file );
  if( b_config == nullptr ) {
   std::cerr << exe << ": Block configuration \"" <<  bconf_file
	     << "\" not valid" << std::endl;
   exit( 1 );
   }
  }
 else {
  // TODO: Try to remove this
  std::cout << "Using a default Block configuration" << std::endl;
  b_config = default_configure_UCBlock( block );
  }

 b_config->apply( block );

 // Configure Solver- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 BlockSolverConfig * s_config;
 s_config = get_blocksolverconfig( sconf_file );
 if( s_config == nullptr ) {
  std::cerr << exe << ": Solver configuration \"" << sconf_file
	    << "\" not valid" << std::endl;
  exit( 1 );
  }

 s_config->apply( block );

 // write nc4 problem - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 if( writeprob )
  write_nc4problem( block , b_config , s_config );

 // solve - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 std::cout.setf( std::ios::scientific, std::ios::floatfield );
 std::cout << std::setprecision( 8 );
 int status = solve_all( block );

 // print the results - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 if( ( ! dryrun ) && ( status == 0 ) )
  print_UCBlock_solver_results( block , solution_output_type );

 return( 0 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*---------------------- End File ucblock_solver.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
