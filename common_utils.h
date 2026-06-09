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

 return( ( std::filesystem::path( prefix ) / p ).lexically_normal().string()
	 );
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

/*--------------------------------------------------------------------------*/
/// prints the tool description and usage

void docopt( void );

/*--------------------------------------------------------------------------*/
/// processes any one of the default command-line arguments

bool process_standard_arg( int opt );

/*--------------------------------------------------------------------------*/
/// processes all default command-line arguments

void process_args( int argc , char ** argv );

/*--------------------------------------------------------------------------*/
/// processes all command-line arguments, with a tool-specific callback
/** Drives the standard getopt_long loop, calling \p custom_arg(opt) for
 * every option that process_standard_arg() does not recognise. \p custom_arg
 * must return true if it consumed \p opt, false otherwise (which prints the
 * usage hint and exits 1). Sets \p exe and \p filename as side effects, and
 * resolves the standard Configuration filenames with \p conf_prefix. */

void process_args( int argc , char ** argv ,
                   bool ( *custom_arg )( int opt ) );

/*--------------------------------------------------------------------------*/
/// custom terminate function to print the exception message

void smspp_terminate( void );

/*--------------------------------------------------------------------------*/
/// get Block from file

Block * get_Block( const std::string & b_file );

/*--------------------------------------------------------------------------*/
/// get Block from group

Block * get_Block( const netCDF::NcGroup & group );

/*--------------------------------------------------------------------------*/
/// gets a Configuration from a Configuration file

Configuration * get_config( const std::string & conf_file );

/*--------------------------------------------------------------------------*/
/// gets a BlockConfig from a BlockConfig file

BlockConfig * get_blockconfig( const std::string & conf_file );

/*--------------------------------------------------------------------------*/
/// gets a BlockSolverConfig from a BlockSolverConfig file

BlockSolverConfig * get_blocksolverconfig( const std::string & conf_file );

/*--------------------------------------------------------------------------*/
/// BlockConfig-ure and BlockSolverConfig-ure a Block
/** \p b_config and \p s_config can be either a, respectively, BlockConfig or
 * BlockSolverConfig, or a "meta" Configuration, i.e., a
 *
 *   SimpleConfiguration< std::map< std::string , Configuration * > >
 *
 * then this is interpreted as "the BlockConfig / BlockSolverConfig that are
 * to be set to the Block / all its sub-Block that have that specific
 * classname()". These are all properly apply()-ed to \p block. The
 * BlockSolverConfig[s] are also properly clear()-ed for final cleanup. */

void config_Block( Block * block ,
		   Configuration * b_config , Configuration * s_config );

/*--------------------------------------------------------------------------*/
/// final cleanup by a clear()-ed [meta]BlockSolverConfig
/** This handles both the case where \p s_config is a BlockSolverConfig or
 * a "meta BlockSolverConfig", i.e., a
 *
 *   SimpleConfiguration< std::map< std::string , Configuration * > >
 */

void cleanup_bsc( Block * block , Configuration * s_config );

/*--------------------------------------------------------------------------*/
/// get Block, BlockConfig and BlockSolverConfig from files, configure all

void get_all( const std::string & b_file , const std::string & bc_file ,
              const std::string & bsc_file , Block * & block ,
              Configuration * & s_config );

/*--------------------------------------------------------------------------*/
/**get Block from group, BlockConfig and BlockSolverConfig from files,
 * configure all */

void get_all( const netCDF::NcGroup & group , const std::string & bc_file ,
              const std::string & bsc_file , Block * & block ,
              Configuration * & s_config );

/*--------------------------------------------------------------------------*/
/// get Block, BlockConfig and BlockSolverConfig from group, configure all

void get_all( const netCDF::NcGroup & group , Block * & block ,
              Configuration * & s_config );

/*--------------------------------------------------------------------------*/
/// prints the status in a human-readable form

void print_status( int status );

/*--------------------------------------------------------------------------*/
/// get and set the initial Solution

void get_initial_Solution( Block * block );

/*--------------------------------------------------------------------------*/
/// get and set the initial State

void get_initial_State( Solver * solver );

/*--------------------------------------------------------------------------*/
/// write the final Solution, using given Configuration if provided
/** Write the Solution currently in the given \p block, using given
 * Configuration \p cfg (if provided, default not) to produce it; bu default
 * append to the file with filename sol_output, rather than replacing it. */

void write_final_Solution( Block * block , Configuration * cfg = nullptr ,
                           bool replace = false );

/*--------------------------------------------------------------------------*/
/// write the final State, by default appending rather than replacing

void write_final_State( Solver * solver , bool replace = false );

/*--------------------------------------------------------------------------*/
/// compute() the Block with all available Solver(s) (unless dry run)

int solve_all( Block * block );

/*--------------------------------------------------------------------------*/
/// writes a new nc4 problem using the Block and its Configuration(s)

void write_nc4problem( Block * block ,
		       Configuration * b_config , Configuration * s_config );

/** @} ---------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

#endif  //__COMMON_UTILS

/*--------------------------------------------------------------------------*/
/*------------------------- end common_utils.h -----------------------------*/
/*--------------------------------------------------------------------------*/
