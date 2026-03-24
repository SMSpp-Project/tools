/*--------------------------------------------------------------------------*/
/*--------------------------- common_utils.h -------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Some common utilities for SMS++ tools.
 *
 * The file defines a common standard for the command-line arguments that can
 * be used by SMS++ "main" files that need to load some Block, its
 * corresponding BlockConfig and BlockSolverConfig, make the configuration,
 * run some Solver, collect the results. Specific support is given for
 * operations like:
 *
 * - set the initial State of the Solver;
 *
 * - load an initial Solution into the Block, save the final Solution;
 *
 * - do not really run the optimization ("dry run")
 *
 * Also, a special "meta-configuration" mode is supported for both the
 * BlockConfig and the BlockSolverConfig: if the specified Configuration is
 * not really a BlockConfig / BlockSolverConfig, but rather a
 *
 *   SimpleConfiguration< std::map< std::string , Configuration * > >
 *
 * then this is interpreted as "the BlockConfig / BlockSolverConfig that are
 * to be set to the Block / all its sub-Block that have that specific
 * classname()". That is, if the SimpleConfiguration< ... > contains, say,
 *
 *    { { "UCBlock" , < pointer to BC1 > } ,
 *      { "DCNetworkBlock" , < pointer to BC2 > } }
 *
 * then the Block is scanned, and all its sub-Block (possibly, itself) that
 * are UCBlock are BlockConfig-ured with (a clone() to) BC1 while all the its
 * sub-Block (...) that are DCNetworkBlock are BlockConfig-ured with (...)
 * BC2; analogously for the BlockSolverConfig (except there is no need for
 * clone()-ing).
 *
 * Note: in the  "meta-configuration" mode, BlockSolverConfig are not properly
 * clear()-ed and used for the final cleanup, which is supposed to be
 * acceptable since typically the executable terminates right after it.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Niccolo' Iardella \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni, Niccolo' Iardella, Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#ifndef __COMMON_UTILS
 #define __COMMON_UTILS

#include <getopt.h>      // for getting command line parameters
#include <filesystem>    // for portable path handling
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <Block.h>
#include <BlockSolverConfig.h>
#include <CDASolver.h>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/
/** @name Global variables used by every tool
 *  @{ */

extern std::string docopt_desc;     ///< tool description

extern std::string filename;        ///< input filename
extern std::string bconf_file;      ///< BlockConfig filename
extern std::string sconf_file;      ///< BlockSolverConfig filename
extern std::string state_in_file;   ///< State to be loaded into the Solver
extern std::string state_out_file;  ///< final State of the Solver
extern std::string block_prefix;    ///< prefix for all Block files
extern std::string conf_prefix;     ///< prefix for all Configuration files
extern std::string exe;             ///< name of the executable file
extern std::string sol_input;       ///< filename of input Solution
extern std::string sol_output;      ///< filename of output Solution
extern std::string sol_cfg_file;    ///< filename of output Solution Configuration

extern bool output_solution;   ///< true if solution has be output
extern bool sol_verbose;       ///< if the Solver should be verbose
extern bool writeprob;         ///< if the problem should be written back
extern bool dryrun;            ///< if compute() need not really ba called

extern int verbosity_level;    ///< verbosity level (0 = silent, >0 = verbose output)

/// default short command-line options
extern std::string short_opts;

/// default long command-line options
extern std::vector< option > long_opts;

/// default command-line options help string
extern std::string help;

/** @} ---------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @name Utility functions
 *  @{ */

/// sets solver log to std::cout if verbosity_level >= 2

inline void set_solver_logs( Block * block ) {
 if( ( verbosity_level >= 2 ) && block )
  for( auto solver : block->get_registered_solvers() )
   solver->set_log( &std::cout );
}

/*--------------------------------------------------------------------------*/
/// normalizes a directory prefix in a portable way

inline std::string normalize_prefix( const std::string & prefix )
{
 if( prefix.empty() )
  return( prefix );

 std::filesystem::path p( prefix );
 p = p.lexically_normal();

 auto s = p.string();
 if( s.empty() )
  return( s );

 if( ( s.back() != '/' ) && ( s.back() != '\\' ) )
  s += std::filesystem::path::preferred_separator;

 return( s );
 }

/*--------------------------------------------------------------------------*/
/// resolves a filename against a prefix in a portable way

inline std::string resolve_with_prefix( const std::string & prefix ,
                                        const std::string & name )
{
 if( name.empty() )
  return( name );

 std::filesystem::path p( name );
 if( p.is_absolute() )
  return( p.lexically_normal().string() );

 if( prefix.empty() )
  return( p.lexically_normal().string() );

 return( ( std::filesystem::path( prefix ) / p ).lexically_normal().string() );
 }

/*--------------------------------------------------------------------------*/
/// gets the name of the executable from its full path

inline std::string get_filename( const std::string & fullpath )
{
 std::size_t found = fullpath.find_last_of( "/\\" );
 return( fullpath.substr( found + 1 ) );
 }

/*--------------------------------------------------------------------------*/
/// gets an option as a string, converts it to long

inline long get_long_option( char * end = nullptr )
{
 errno = 0;
 long option = std::strtol( optarg , &end , 10 );
 if( ( ! optarg ) || ( ( option = std::strtol( optarg , &end , 10 ) ) ,
                       ( errno || ( end && *end ) ) ) ) {
  option = -1;
  }
 return( option );
 }

/*--------------------------------------------------------------------------*/
/// open a netCDF file for reading, returns its type

int read_open_netCDF( netCDF::NcFile & f , std::string fn );

void docopt( void );

bool process_standard_arg( int opt );

void process_args( int argc , char ** argv );

void smspp_terminate( void );

BlockConfig * get_blockconfig( const std::string & conf_file );

BlockSolverConfig * get_blocksolverconfig( const std::string & conf_file );

void config_Block( Block * block , BlockConfig * b_config ,
                   BlockSolverConfig * s_config );

void get_all( const std::string & b_file , const std::string & bc_file ,
              const std::string & bsc_file , Block * & block ,
              BlockConfig * & b_config , BlockSolverConfig * & s_config );

void get_all( const netCDF::NcGroup & group , const std::string & bc_file ,
              const std::string & bsc_file , Block * & block ,
              BlockConfig * & b_config , BlockSolverConfig * & s_config );

void get_all( const netCDF::NcGroup & group , Block * & block ,
              BlockConfig * & b_config , BlockSolverConfig * & s_config );

void print_status( int status );

void get_initial_Solution( Block * block );

void get_initial_State( Solver * solver );

void write_final_Solution( Block * block , Configuration * cfg = nullptr ,
                           bool replace = false );

void write_final_State( Solver * solver , bool replace = false );

int solve_all( Block * block );

void write_nc4problem( Block * block , BlockConfig * b_config ,
                       BlockSolverConfig * s_config );

/** @} ---------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

#endif  //__COMMON_UTILS

/*--------------------------------------------------------------------------*/
/*------------------------- end common_utils.h -----------------------------*/
/*--------------------------------------------------------------------------*/
