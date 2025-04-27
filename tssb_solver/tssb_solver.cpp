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
 * of the TwoStageStochasticBlock. This can be either a BlockFile or a
 * a ProbFile. The BlockFile can contain any number of child groups, each one
 * describing an TwoStageStochasticBlock, each of which is then solved with
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

#include <BlockSolverConfig.h>
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

const std::string my_short_opts = "d:";

const std::vector< option > my_long_opts = {
  { "output-dir" ,               required_argument , nullptr , 'd' } ,
  { nullptr ,                    no_argument ,       nullptr , 0 }
  };

const std::string my_help =
 "  -d, --output-dir                directory where solutions are written\n";

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

void process_my_args( int argc , char ** argv ) {
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
  // case 'd': output_solution_directory = std::string( optarg ); break;
  case '?' : // Unrecognized option
  default :
   std::cout << "Try " << exe << "' --help' for more information" << std::endl;
   exit( 1 );
  }
 } // end( while( true ) )

 if( optind < argc ) // last argument == [InvestmentBlock] filename
  filename = std::string( argv[ optind ] );
 else {
  std::cout << exe << ": no input file" << std::endl
   << "Try " << exe << "' --help' for more information" << std::endl;
  exit( 1 );
 }
} // end( process_my_args )

/*--------------------------------------------------------------------------*/

void process_prob_file( const netCDF::NcFile & file ) {
 auto problems = file.getGroups();

 for( auto & problem : problems ) { // for each problem descriptor:
  auto & problem_group = problem.second;

  // Deserialize block
  auto block_group = problem_group.getGroup( "Block" );
  auto tss_block = new TwoStageStochasticBlock;
  tss_block->deserialize( block_group );

  // Configure block
  auto block_config_group = problem_group.getGroup( "BlockConfig" );
  auto block_config = static_cast< BlockConfig * >(
   BlockConfig::new_Configuration( block_config_group ) );
  if( ! block_config )
   throw( std::logic_error( "invalid BlockConfig group" ) );

  block_config->apply( tss_block );
  block_config->clear();

  // Configure solver
  auto solver_config_group = problem_group.getGroup( "BlockSolver" );
  auto block_solver_config = static_cast< BlockSolverConfig * >(
   BlockSolverConfig::new_Configuration( solver_config_group ) );
  if( ! block_solver_config )
   throw( std::logic_error( "invalid BlockSolver group" ) );
  block_solver_config->apply( tss_block );
  block_solver_config->clear();

  std::cout << "Problem: " << problem.first << std::endl;

  // Solve
  int status = solve_all( tss_block );

  // Print the results
  // if( status == 0 ) print_UCBlock_solver_results( tss_block , solution_output_type );

  // Destroy the Block and the Configurations
  block_config->apply( tss_block );
  delete( block_config );

  block_solver_config->apply( tss_block );
  delete( block_solver_config );

  delete( tss_block );
 }
}

/*--------------------------------------------------------------------------*/

void process_block_file( const netCDF::NcFile & file ) {
 // BlockConfig
 BlockConfig * given_block_config = nullptr;
 if( bconf_file.empty() )
  std::cout << "Block configuration was not provided, "
   "using default configuration" << std::endl;
 else if( ( given_block_config = get_blockconfig( bconf_file ) ) )
  std::cout << "Using Block configuration in " << bconf_file << std::endl;
 else {
  std::cerr << "Block Configuration " << bconf_file << " invalid" << std::endl;
  delete( given_block_config );
  exit( 1 );
 }

 BlockConfig * block_config = nullptr;
 if( given_block_config ) {
  block_config = given_block_config->clone();
  block_config->clear();
 }

 // BlockSolverConfig
 bool block_solver_config_provided = true;
 auto solver_config = get_blocksolverconfig( sconf_file );
 if( ! solver_config ) {
  std::cerr << "The Solver configuration is not valid." << std::endl;
  exit( 1 );
 }

 auto cleared_solver_config = solver_config->clone();
 cleared_solver_config->clear();

 auto blocks = file.getGroups();
 for( auto block_description : blocks ) { // for each Block descriptor
  // Deserialize the TwoStageStochasticBlock
  auto tss_block = new TwoStageStochasticBlock;
  tss_block->deserialize( block_description.second );

  // Configure the TwoStageStochasticBlock
  if( given_block_config )
   given_block_config->apply( tss_block );
  else {
   // configure_Blocks( tss_block , feasibility_tolerance , relative_violation );

   if( ! block_solver_config_provided ) {
    // block_config = build_BlockConfig( tss_block );
    block_config->apply( tss_block );
    block_config->clear();
   }
  }

  // Configure the Solver
  solver_config->apply( tss_block );

  // Solve
  int status = solve_all( tss_block );

  // Print the results
  // if( status == 0 ) print_UCBlock_solver_results( tss_block , solution_output_type );

  // Destroy the Block and the Configurations
  if( block_config )
   block_config->apply( tss_block );
  if( ! given_block_config ) {
   delete( block_config );
   block_config = nullptr;
  }

  cleared_solver_config->apply( tss_block );

  delete( tss_block );
 }

 delete( block_config );
 delete( given_block_config );
 delete( solver_config );
 delete( cleared_solver_config );
}

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv ) {
 // append new options to default ones- - - - - - - - - - - - - - - - - - - -
 // note that the last nullptr record in long_opts is overwritten since the
 // new one is further down from there

 docopt_desc = "SMS++ TSSB solver.\n";
 short_opts.append( my_short_opts );
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_long_opts.begin() , my_long_opts.end() );
 help.append( my_help );

 // process command-line arguments- - - - - - - - - - - - - - - - - - - - - -

 process_my_args( argc , argv );

 // open the file - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 netCDF::NcFile file;
 auto type = read_open_netCDF( file , filename );

 // process the file- - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 switch( type ) {
 case eProbFile : std::cout << filename << " is a problem file, "
   << "ignoring Block/Solver Configuration(s)..." << std::endl;
  process_prob_file( file );
  break;
 case eBlockFile : std::cout << filename << " is a Block file" << std::endl;
  process_block_file( file );
  break;
 default : std::cerr << filename << " is not a valid SMS++ file" << std::endl;
  exit( 1 );
 }

 return( 0 );
} // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File tssb_solver.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
