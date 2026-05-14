/*--------------------------------------------------------------------------*/
/*-------------------------- File tssb_solver.cpp --------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * This is a convenient tool for solving a TwoStageStochasticBlock.
 * The description of the TwoStageStochasticBlock must be given in a netCDF
 * file. This tool can be executed as follows:
 *
 *   ./tssb_solver [-s] [-e] [-m NUMBER] [-B FILE] [-S FILE] [-p PATH]
 *                 [-c PATH] < nc4-file >
 *
 * The only mandatory argument is the netCDF file containing the description
 * of the TwoStageStochasticBlock. This can be either a BlockFile or
 * a ProbFile. The BlockFile can contain any number of child groups, each one
 * describing a TwoStageStochasticBlock, each of which is then solved with
 * the same BlockConfig and BlockSolverConfig. The ProbFile can also contain
 * any number of child groups, each one having the description of a
 * TwoStageStochasticBlock alongside the description of a BlockConfig and a
 * BlockSolverConfig for the TwoStageStochasticBlock; thus, every
 * TwoStageStochasticBlock is solved with these specified BlockConfig and
 * BlockSolverConfig.
 *
 * The -c option specifies the prefix to the paths to all configuration
 * files. This means that if PATH is the value passed to the -c option, then
 * the name (or path) to each configuration file will be prepended by
 * PATH. The -p option specifies the prefix to the paths to all files
 * specified by the attribute "filename" in the input netCDF file.
 *
 * The -B and -S options are only considered if the given netCDF file is a
 * BlockFile. The -B option specifies a BlockConfig file to be applied to
 * every TwoStageStochasticBlock; while the -S option specifies a
 * BlockSolverConfig file for every TwoStageStochasticBlock. If each of these
 * options is not provided when the given netCDF file is a BlockFile, then
 * default configurations are considered.
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli, Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <iomanip>
#include <iostream>

#include <TwoStageStochasticBlock.h>

#include "common_utils.h"
#include "ucblock_utils.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

// Name of Configuration files for each component of the Lagrangian dual of
// the UCBlock
const std::string thermal_config_filename = "TUBSCfg.txt";
const std::string hydro_config_filename = "HSUBSCfg.txt";
const std::string other_unit_config_filename = "OUBSCfg.txt";
const std::string default_config_filename = "LPBSCfg.txt";

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 // tssb_solver has no tool-specific options
 return( false );
 }

/*--------------------------------------------------------------------------*/

void process_prob_file( const netCDF::NcFile & file )
{
 auto problems = file.getGroups();

 for( auto & problem : problems ) {  // for each problem descriptor:
  Block * block;
  Configuration * s_config;
  get_all( problem.second , block , s_config );

  if( ! dynamic_cast< TwoStageStochasticBlock * >( block ) ) {
   std::cout << "Error: " << problem.first
	     << " not a TwoStageStochasticBlock" << std::endl;
   exit( 1 );
   }

  std::cout << "Problem: " << problem.first << std::endl;

  set_solver_logs( block );

  // Solve
  solve_all( block );

  // cleanup
  cleanup_bsc( block , s_config );
  delete s_config;
  delete block;
  }
 }

/*--------------------------------------------------------------------------*/

void process_block_file( const netCDF::NcFile & file )
{
 auto blocks = file.getGroups();

 for( auto & b : blocks ) {  // for each Block descriptor
  Block * block;
  Configuration * s_config;
  get_all( b.second , bconf_file , sconf_file , block , s_config );

  if( ! dynamic_cast< TwoStageStochasticBlock * >( block ) ) {
   std::cout << "Error: " << b.first
	     << " not a TwoStageStochasticBlock" << std::endl;
   exit( 1 );
   }

  set_solver_logs( block );

  // Solve
  solve_all( block );

  // cleanup
  cleanup_bsc( block , s_config );
  delete s_config;
  delete block;
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // append new options to default ones- - - - - - - - - - - - - - - - - - - -
 // note that the last nullptr record in long_opts is overwritten since the
 // new one is further down from there

 docopt_desc = "SMS++ TSSB solver.\n";

 // process command-line arguments- - - - - - - - - - - - - - - - - - - - - -

 process_args( argc , argv , process_specific_arg );

 // open the file - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 netCDF::NcFile file;
 auto type = read_open_netCDF( file , filename );

 // process the file- - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 switch( type ) {
  case( eProbFile ):
   std::cout << filename << " is a problem file, "
	     << "ignoring Block/Solver Configuration(s)..." << std::endl;
   process_prob_file( file );
   break;
  case( eBlockFile ):
   std::cout << filename << " is a Block file" << std::endl;
   process_block_file( file );
   break;
  default :
   std::cerr << filename << " is not a valid SMS++ file" << std::endl;
   exit( 1 );
  }

 return( 0 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File tssb_solver.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
