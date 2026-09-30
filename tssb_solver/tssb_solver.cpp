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
 *                 [-c PATH] [-k] [--mpi-procs N] < nc4-file >
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
 * With --mpi-procs N, the tool relaunches itself via mpirun -np N before
 * opening the input file. The worker processes run the normal application
 * path, including Solver-controlled MPI initialization. Without the option,
 * startup is unchanged. Open MPI / PMI / PMIx rank environment variables
 * suppress relaunch under an external launcher; --mpi-worker explicitly
 * suppresses it for launchers not covered by detection. An existing MPI job
 * keeps its process count even when --mpi-procs is supplied.
 *
 * Automatic launch requires POSIX execvp and mpirun on PATH, compatible with
 * the MPI library used by the Solver. The executable and input/configuration
 * files must be accessible to all workers. This option only arranges launch:
 * the chosen Solver must support MPI. Solution output (-O) is written only
 * by rank 0, after all ranks participate in solution retrieval. Other output
 * files (-a, -n) are not coordinated between ranks by this tool.
 *
 * The -k option solves the Benders form of each TwoStageStochasticBlock
 * rather than the TwoStageStochasticBlock itself [see
 * TwoStageStochasticBlock::get_Benders_form()]: the BlockConfig is applied
 * to the TwoStageStochasticBlock, the Benders form is assembled around it,
 * and the BlockSolverConfig is applied to the root of the form, which is
 * where a Benders decomposition Solver is attached. The form is given back
 * once solved, and the Solution written (-O) is that of the
 * TwoStageStochasticBlock, as without -k. It is only available for a
 * BlockFile.
 *
 * A MultiStageStochasticBlock is a TwoStageStochasticBlock, and this tool
 * solves it as well when it is built with that module: with -k, the
 * sub-Block of the form are the leaves of the scenario tree, which is the
 * Benders form of the problem as long as the only here-and-now Variable are
 * those of the root.
 *
 * A feasible solution out of a Solver that only gives a bound comes from
 * the configuration, not from this tool: a PrimalProximalHeur on the
 * extensive form (strRecoveryBSC) or a BendersDecompositionSolver on the
 * Benders form (strRecoveryBSC) recovers it and reports it as its upper
 * bound.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni, Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <iomanip>
#include <iostream>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <AbstractBlock.h>

#include <TwoStageStochasticBlock.h>

#include "common_utils.h"

#ifdef TSSB_USE_MPI
#include <mpi.h>
#endif

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

bool benders_form = false;  ///< solve the Benders form (-k)
int mpi_processes = 0;      ///< automatic MPI launch is opt-in
bool mpi_worker = false;    ///< skip relaunch in processes started by mpirun

enum { opt_mpi_procs = 1000 , opt_mpi_worker };

const std::string my_short_opts = "k";

const std::vector< option > my_long_opts = {
  { "benders" , no_argument , nullptr , 'k' },
  { "mpi-procs" , required_argument , nullptr , opt_mpi_procs },
  { "mpi-worker" , no_argument , nullptr , opt_mpi_worker }
  };

const std::string my_help =
 "  -k, --benders                   solve the Benders form of the problem,\n"
 "                                  the Solver being attached to its root\n"
 "      --mpi-procs N               launch under mpirun with N processes\n"
 "                                  unless already launched under MPI\n"
 "      --mpi-worker                skip relaunch (for MPI workers)";

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 switch( opt ) {  // non-standard options
  case 'k': benders_form = true; return( true );
  case opt_mpi_worker: mpi_worker = true; return( true );
  case opt_mpi_procs: {
   // Reject signs, whitespace, zero and overflow rather than passing them
   // through to mpirun. getopt_long supplies optarg for this option.
   const std::string value( optarg );
   char * end = nullptr;
   errno = 0;
   const auto count = std::strtol( optarg , &end , 10 );
   if( value.empty() ||
       value.find_first_not_of( "0123456789" ) != std::string::npos ||
       errno == ERANGE || *end || count <= 0 || count > INT_MAX ) {
    std::cerr << "--mpi-procs requires a positive integer <= " << INT_MAX
              << std::endl;
    std::exit( EXIT_FAILURE );
    }
   mpi_processes = static_cast< int >( count );
   return( true );
   }
  default: return( false );
  }
 }

/*--------------------------------------------------------------------------*/

// MPI_Initialized() does not detect a launcher: it is false until MPI_Init().
// Check rank markers used by Open MPI and PMI/PMIx launchers. A scheduler
// allocation alone (e.g. SLURM_JOB_ID) is not evidence of an MPI worker.
static bool detected_mpi_launch()
{
 for( const auto name : { "OMPI_COMM_WORLD_RANK", "PMI_RANK", "PMIX_RANK" } )
  if( const auto value = std::getenv( name ); value && *value )
   return( true );
 return( false );
 }

// Evaluated after solution retrieval: PIPS has initialized MPI by then.
static bool is_solution_writer()
{
#ifdef TSSB_USE_MPI
 int initialized = 0;
 MPI_Initialized( & initialized );
 if( initialized ) {
  int rank = 0;
  if( MPI_Comm_rank( MPI_COMM_WORLD , & rank ) != MPI_SUCCESS )
   throw( std::runtime_error( "Cannot determine the solution output rank" ) );
  return( rank == 0 );
  }
#endif
 // Also support launched processes using a non-MPI Solver, and builds
 // without MPI linkage, using the same launchers recognized at startup.
 for( const auto name : { "OMPI_COMM_WORLD_RANK", "PMI_RANK", "PMIX_RANK" } )
  if( const auto value = std::getenv( name ); value && *value ) {
   char * end = nullptr;
   errno = 0;
   const auto rank = std::strtol( value , & end , 10 );
   if( errno || *end || rank < 0 )
    throw( std::runtime_error( "Invalid MPI rank environment variable" ) );
   return( rank == 0 );
   }
 return( true );  // ordinary serial execution
 }

static int relaunch_under_mpi( const std::vector< std::string > & original )
{
#ifdef _WIN32
 (void) original;
 std::cerr << "Automatic MPI launch requires POSIX execvp; launch with "
              "mpiexec and --mpi-worker on Windows." << std::endl;
 return( EXIT_FAILURE );
#else
 // Pass argv[0] unchanged, just as in: mpirun -np N ./tssb_solver.
 // mpirun handles executable lookup; no shell is involved.
 std::vector< std::string > arguments = {
  "mpirun", "-np", std::to_string( mpi_processes ), original.front(),
  "--mpi-worker"
  };
 // Retain all original options, including --mpi-procs: --mpi-worker takes
 // precedence. Insert our marker before any original '--' end-of-options.
 arguments.insert( arguments.end() , original.begin() + 1 , original.end() );
 std::vector< char * > command;
 for( auto & argument : arguments ) command.push_back( argument.data() );
 command.push_back( nullptr );
 execvp( command.front() , command.data() );
 const auto error = errno;
 std::cerr << "Cannot launch mpirun: " << std::strerror( error ) << std::endl;
 return( EXIT_FAILURE );
#endif
 }

/*--------------------------------------------------------------------------*/

void process_prob_file( const netCDF::NcFile & file )
{
 if( benders_form ) {
  std::cout << "Error: the Benders form (-k) needs a Block file, whose "
               "BlockSolverConfig is given by -S" << std::endl;
  exit( 1 );
  }

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
  solve_all( block , is_solution_writer );

  // cleanup
  cleanup_bsc( block , s_config );
  delete s_config;
  delete block;
  }
 }

/*--------------------------------------------------------------------------*/

/// solves the Benders form of the TwoStageStochasticBlock in \p group
/** The BlockConfig is applied to the TwoStageStochasticBlock, whose abstract
 * representation is then generated, since the form is read off it; the
 * BlockSolverConfig is applied to the root of the form. */

static void solve_Benders_form( const std::string & name ,
                                const netCDF::NcGroup & group )
{
 require_solver_config( sconf_file );
 auto block = get_Block( group );
 auto tssb = dynamic_cast< TwoStageStochasticBlock * >( block );
 if( ! tssb ) {
  std::cout << "Error: " << name << " not a TwoStageStochasticBlock"
            << std::endl;
  exit( 1 );
  }

 auto b_config = get_config( bconf_file );
 config_Block( block , b_config , nullptr );
 delete b_config;

 tssb->generate_abstract_variables();
 tssb->generate_abstract_constraints();
 tssb->generate_objective();

 auto form = tssb->get_Benders_form();
 if( ! form ) {
  std::cout << "Error: " << name << " declares no here-and-now Variable, "
               "hence it has no Benders form" << std::endl;
  exit( 1 );
  }

 auto s_config = get_config( sconf_file );
 config_Block( form , nullptr , s_config );

 set_solver_logs( form );

 // Solve, the Solution being that of the TwoStageStochasticBlock
 solve_all( form , is_solution_writer , false );

 // cleanup
 cleanup_bsc( form , s_config );
 delete s_config;
 tssb->give_back_Benders_form( form );

 // the leaves are back in the TwoStageStochasticBlock, with the values
 // the Solver has written into them
 if( is_solution_writer() )
  write_final_Solution( tssb );

 delete block;
 }

/*--------------------------------------------------------------------------*/

void process_block_file( const netCDF::NcFile & file )
{
 auto blocks = file.getGroups();

 for( auto & b : blocks ) {  // for each Block descriptor
  if( benders_form ) {
   solve_Benders_form( b.first , b.second );
   continue;
   }

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
  solve_all( block , is_solution_writer );

  // cleanup
  cleanup_bsc( block , s_config );

  delete s_config;
  delete block;
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // getopt_long may permute argv; preserve its original order for relaunch.
 const std::vector< std::string > original_arguments( argv , argv + argc );

 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // append new options to default ones- - - - - - - - - - - - - - - - - - - -
 // note that the last nullptr record in long_opts is overwritten since the
 // new one is further down from there

 docopt_desc =
  "SMS++ TSSB solver: loads a two-stage stochastic problem (a\n"
  "TwoStageStochasticBlock) and solves it with the Solvers of its\n"
  "BlockSolverConfig.\n";
 docopt_args =
  "  <file>    SMS++ netCDF file (.nc4) holding a TwoStageStochasticBlock:\n"
  "            a Block file, or a problem file, whose own configuration is\n"
  "            then used and -B and -S are ignored\n";
 docopt_examples =
  "  tssb_solver instance.nc4\n"
  "      solve the deterministic equivalent with a :MILPSolver\n"
  "  tssb_solver --mpi-procs 3 -S PIPSConfig.txt instance.nc4\n"
  "      launch three MPI processes using your PIPS solver configuration\n"
  "  tssb_solver -S TSSBSCfg-LD.txt instance.nc4\n"
  "      solve the Lagrangian dual of the scenario decomposition, whose\n"
  "      master problem needs CPLEX or Gurobi\n"
  "  tssb_solver -k -S TSSBSCfg-BDS.txt instance.nc4\n"
  "      solve the Benders form of the problem with the\n"
  "      BendersDecompositionSolver of TSSBSCfg-BDS.txt\n"
  "  tssb_solver -c myconfig/ instance.nc4\n"
  "      use the Configuration files in myconfig/, e.g. a modified copy\n"
  "      of the installed ones\n";

 // Configuration files live in config/ by default; an explicit -c overrides
 // this. Default -B / -S so a plain run needs neither: TSSBCfg.txt is the
 // BlockConfig (anchor/sequential formulation) and TSSBSCfg.txt the
 // BlockSolverConfig for the TwoStageStochasticBlock
 conf_prefix = "config/";
 default_bconf_name = "TSSBCfg.txt";
 default_sconf_name = "TSSBSCfg.txt";

 // process command-line arguments- - - - - - - - - - - - - - - - - - - - - -

 short_opts.append( my_short_opts );
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_long_opts.begin() , my_long_opts.end() );
 help.append( my_help );

 process_args( argc , argv , process_specific_arg );

 // This must precede loading Blocks or constructing Solvers, which may
 // initialize MPI. Every worker continues through the normal solve path.
 if( mpi_processes && ! mpi_worker && ! detected_mpi_launch() )
  return( relaunch_under_mpi( original_arguments ) );

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
