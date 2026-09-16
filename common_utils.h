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
#include <iomanip>
#include <filesystem>    // for portable path handling
#include <functional>
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
extern std::string docopt_args;     ///< description of the <file> argument
extern std::string docopt_examples; ///< usage examples printed by --help

extern std::string filename;        ///< input filename
extern std::string bconf_file;      ///< BlockConfig filename
extern std::string sconf_file;      ///< BlockSolverConfig filename

/// conventional default BlockConfig filename (-B), when reachable
extern std::string default_bconf_name;
/// conventional default BlockSolverConfig filename (-S), when reachable
/** The name process_args() falls back to when -S is not given on the command
 * line; defaults to the common "BSCfg.txt", but a tool whose top-level Solver
 * is configured by a differently named file may override it before calling
 * process_args() (e.g. investmentblock_solver, whose InvestmentBlock is solved
 * by a BundleSolver configured in BSPar.txt, while BSCfg.txt configures the
 * inner Block of the InvestmentFunction via strInnerBSC). */
extern std::string default_sconf_name;
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
extern std::string prob_file;  ///< filename of the problem written back (-n)
extern char input_format;      ///< native format of the input file (-f)
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
/// removes one of the default command-line options
/** Removes the long options and the line of the help of the default option
 * \p opt, so that a tool can give that letter a meaning of its own; to be
 * called before the tool adds its options. */

void drop_standard_option( char opt );

/*--------------------------------------------------------------------------*/
/// adds the -f option, the native format of the input file, to the tool
/** For the tools whose input file can also be in the native text format(s)
 * of their Block: -f <c> sets input_format to c, and \p formats is added to
 * the help to describe the formats; to be called before process_args(). */

void add_format_option( const std::string & formats );

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
 * resolves the standard Configuration filenames with \p conf_prefix. When -c
 * is not given and none of the Configuration files is found with the prefix
 * of the tool, \p conf_prefix becomes installed_config_dir(), if any: the
 * whole configuration then comes from the installed directory. */

void process_args( int argc , char ** argv ,
                   bool ( *custom_arg )( int opt ) );

/*--------------------------------------------------------------------------*/
/// custom terminate function to print the exception message

void smspp_terminate( void );

/*--------------------------------------------------------------------------*/
/// get Block from file

Block * get_Block( const std::string & b_file );

/*--------------------------------------------------------------------------*/
/// get Block from file, an SMS++ netCDF one or one in a native format
/** If \p b_file is an SMS++ netCDF file, or has the "<file>[i]" form that
 * selects one of its Block, the Block is deserialized from it as by
 * get_Block( b_file ). Otherwise, a Block of class \p classname is created
 * by the factory and load()-ed from the file in its native format \p frmt
 * (0 for the default one of the Block); an empty \p classname means that
 * only the netCDF format is accepted. The file is looked up under the -p
 * prefix in both cases. */

Block * get_Block( const std::string & b_file , const std::string & classname ,
                   char frmt );

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
/// gets the value of a string parameter of a ComputeConfig
/** Returns the value of the \p par_name string parameter of \p compute_config,
 * or an empty string if it is not present. */

std::string get_str_par( const ComputeConfig * compute_config ,
                         const std::string & par_name );

/*--------------------------------------------------------------------------*/
/// gets the value of an integer parameter of a ComputeConfig
/** Returns the value of the \p par_name integer parameter of \p compute_config,
 * or Inf< int >() if it is not present. */

int get_int_par( const ComputeConfig * compute_config ,
                 const std::string & par_name );

/*--------------------------------------------------------------------------*/
/// removes a string parameter from a ComputeConfig, if present
/** Erases the \p par_name string parameter from \p compute_config. Used for
 * "pseudo-parameters" that a tool consumes but that are not real Solver
 * parameters (e.g. strInnerBSC), so they are not passed on to the Solver when
 * the ComputeConfig is applied. */

void erase_str_par( ComputeConfig * compute_config ,
                    const std::string & par_name );

/*--------------------------------------------------------------------------*/
/// reports a Configuration file in use, mirroring the "X is a Block file" log
/** When verbosity_level >= 1, prints which Configuration file is being used
 * for \p what (e.g. "BlockSolverConfig (-S)"), resolved against conf_prefix,
 * or a "no <what>" note when \p file is empty. This lets a run with defaults
 * make clear which files it picked up. A no-op when verbosity_level < 1. */

void report_config_file( const std::string & what , const std::string & file );

/*--------------------------------------------------------------------------*/
/// resolves a conventional default Configuration file name
/** Returns \p name if a file with that name is actually reachable at the
 * conf_prefix location (the -c prefix, which a plain run points at the
 * config/ directory); returns an empty string otherwise. This is the lookup
 * behind the optional -B / -S defaults of process_args(), and is reused by
 * tools that have further conventional Configuration files (e.g. the inner
 * BlockSolverConfig of investmentblock_solver). */

std::string default_config_file( const std::string & name );

/*--------------------------------------------------------------------------*/
/// the Configuration directory installed with the tool, if any
/** Returns the directory holding the Configuration files installed together
 * with the tool, as a prefix ending with a separator, or an empty string if
 * the tool has none. The directory is found relative to the executable (the
 * relative path is the SMSPP_TOOL_CONFIG_DIR macro, set by CMake for the
 * tools that install their config/ directory), so that the installed tree
 * can be moved as a whole; symbolic links to the executable are resolved
 * first. process_args() falls back to this directory when -c is not given
 * and no Configuration file is found in the current one. */

std::string installed_config_dir( void );

/*--------------------------------------------------------------------------*/
/// BlockConfig-ure and BlockSolverConfig-ure a Block
/** \p b_config and \p s_config can be either a, respectively, BlockConfig or
 * BlockSolverConfig, or a "meta" Configuration, i.e., a
 *
 *   SimpleConfiguration< std::map< std::string , Configuration * > >
 *
 * then this is interpreted as "the BlockConfig / BlockSolverConfig that are
 * to be set to the Block / all its sub-Block that have that specific
 * classname()", with the special entry "*" (if any) acting as the default
 * for the Block whose classname() does not match any other entry. These are
 * all properly apply()-ed to \p block. The BlockSolverConfig[s] are also
 * properly clear()-ed for final cleanup. */

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
/// require that a BlockSolverConfig was provided (-S); throw otherwise
/** Throws std::invalid_argument if \p bsc_file is empty. A solver
 * configuration is needed to solve anything; when -S is not given,
 * process_args() falls back to the conventional BSCfg.txt if it is reachable
 * through the -c prefix, so this only fires when no configuration can be
 * found at all. The BlockConfig (-B) is not required here, as a generic
 * Block may be solved as deserialized. */

void require_solver_config( const std::string & bsc_file );

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
/** The file is the one given to -n or, if that is empty, the input filename
 * with its extension replaced by "_problem.nc4". */

void write_nc4problem( Block * block ,
		       Configuration * b_config , Configuration * s_config );

/*--------------------------------------------------------------------------*/
/// the run of a tool that solves the Block of class B in the input file
/** Reads the Block of class \p classname (of type B) from the input file,
 * either an SMS++ netCDF file or one in the native format input_format (see
 * get_Block( b_file , classname , frmt )), unless \p native is false, in
 * which case only the netCDF format is accepted; configures the Block with
 * -B and -S, writes the problem back if -n is given, solves it with all the
 * Solver(s) and cleans up; returns the exit status of the tool. If given,
 * \p prepare is called on the Block between its BlockConfig and its
 * BlockSolverConfig, i.e., before any Solver is registered to it. */

template< class B >
int solve_Block_file( const std::string & classname , bool native = true ,
                      const std::function< void( B * ) > & prepare = {} )
{
 Block * block = get_Block( filename , native ? classname : std::string() ,
                            input_format );

 auto b = dynamic_cast< B * >( block );
 if( ! b ) {
  std::cerr << exe << ": " << filename << " is not a " << classname
            << std::endl;
  exit( 1 );
  }

 require_solver_config( sconf_file );

 auto b_config = get_config( bconf_file );
 auto s_config = get_config( sconf_file );

 config_Block( block , b_config , nullptr );
 if( prepare )
  prepare( b );
 config_Block( block , nullptr , s_config );

 if( writeprob )
  write_nc4problem( block , b_config , s_config );

 std::cout.setf( std::ios::scientific , std::ios::floatfield );
 std::cout << std::setprecision( 8 );
 solve_all( block );

 cleanup_bsc( block , s_config );
 delete s_config;
 delete b_config;
 delete block;

 return( 0 );
 }

/** @} ---------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

#endif  //__COMMON_UTILS

/*--------------------------------------------------------------------------*/
/*------------------------- end common_utils.h -----------------------------*/
/*--------------------------------------------------------------------------*/
