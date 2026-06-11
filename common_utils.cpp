/*--------------------------------------------------------------------------*/
/*--------------------------- common_utils.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of some common utilities for SMS++ tools whose API is
 * defined in common_utils.h.
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

#include <iostream>

#include <Block.h>
#include <BlockSolverConfig.h>
#include <Solution.h>

#include "common_utils.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------ MPI SAFE ENVIRONMENT -----------------------------*/
/*--------------------------------------------------------------------------*/
/* Some tools (those using SDDPBlock or InvestmentBlock) call MPI_Init().
 * On systems where Open MPI / UCX are installed but no usable transport is
 * available (no IB, missing UCX vfs.sock, ...), or where the hwloc GL
 * component hangs probing the GPU topology via XOpenDisplay(), the MPI
 * runtime can hang on startup spinning on futex / X11 sockets.
 *
 * To make every tool work out-of-the-box, we pre-seed safe defaults with
 * setenv(..., 0): the third argument is "overwrite = false", so any user
 * who has already exported UCX_TLS / OMPI_MCA_* / HWLOC_COMPONENTS (e.g.
 * on an HPC cluster with a real fabric) keeps full control. This is
 * executed before main() via a static initializer; it is the same
 * mechanism used by tests/common_utils.cpp.                              */

namespace {

void set_default_env( const char * name , const char * value ) {
#ifdef _WIN32
 if( std::getenv( name ) == nullptr )
  _putenv_s( name , value );
#else
 setenv( name , value , 0 );
#endif
}

struct SmsppMpiSafeEnvInit {
 SmsppMpiSafeEnvInit() {
  set_default_env( "UCX_TLS"     , "tcp,self" );
  set_default_env( "OMPI_MCA_btl", "tcp,self" );
  set_default_env( "OMPI_MCA_pml", "ob1"      );
  // the hwloc GL component probes the GPU topology via XOpenDisplay(),
  // which may hang inside MPI_Init(); no SMS++ target has a use for it
  set_default_env( "HWLOC_COMPONENTS", "-gl"  );
  }
 };

static SmsppMpiSafeEnvInit smspp_mpi_safe_env_init_;

}  // anonymous namespace

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/
/** @name Global variables used by every tool
 *  @{ */

std::string docopt_desc {};     ///< tool description

std::string filename {};        ///< input filename
std::string bconf_file {};      ///< BlockConfig filename
std::string sconf_file {};      ///< BlockSolverConfig filename
std::string state_in_file {};   ///< State to be loaded into the Solver
std::string state_out_file {};  ///< final State of the Solver
std::string block_prefix {};    ///< prefix for all Block files
std::string conf_prefix {};     ///< prefix for all Configuration files
std::string exe {};             ///< name of the executable file
std::string sol_input {};       ///< filename of input Solution
std::string sol_output {};      ///< filename of output Solution
std::string sol_cfg_file {};    ///< filename of output Solution Configuration

bool output_solution = false;   ///< true if solution has be output
bool sol_verbose = false;       ///< if the Solver should be verbose
bool writeprob = false;         ///< if the problem should be written back
bool dryrun = false;            ///< if compute() need not really ba called

int verbosity_level = 0;        ///< verbosity level (0 = silent, >0 = verbose output)

/// default short command-line options
std::string short_opts = "a:B:b:p:S:c:on:I:O:C:Dv:h";

/// default long command-line options
std::vector< option > long_opts = {
 { "help"            , no_argument       , nullptr , 'h' } ,
 { "save-state"      , required_argument , nullptr , 'a' } ,
 { "blockcfg"        , required_argument , nullptr , 'B' } ,
 { "load-state"      , required_argument , nullptr , 'b' } ,
 { "prefix"          , required_argument , nullptr , 'p' } ,
 { "solvercfg"       , required_argument , nullptr , 'S' } ,
 { "configdir"       , required_argument , nullptr , 'c' } ,
 { "output-solution" , no_argument       , nullptr , 'o' } ,
 { "nc4problem"      , required_argument , nullptr , 'n' } ,
 { "inputsol"        , required_argument , nullptr , 'I' } ,
 { "outputsol"       , required_argument , nullptr , 'O' } ,
 { "outsolcfg"       , required_argument , nullptr , 'C' } ,
 { "dryrun"          , no_argument       , nullptr , 'D' } ,
 { "verbose"         , optional_argument , nullptr , 'v' } ,
 { nullptr           , no_argument       , nullptr , 0 }
};

/// default command-line options help string
std::string help =
 "  -h, --help                      print this help\n"
 "  -a, --save-state <file>         save State of the Solver\n"
 "  -B, --blockcfg <file>           Block Configuration\n"
 "  -b, --load-state <file>         load State for the Solver\n"
 "  -p, --prefix <path>             the prefix for all Block filenames\n"
 "  -S, --solvercfg <file>          Solver Configuration\n"
 "  -c, --configdir <path>          the prefix for all Config filenames\n"
 "  -I, --inputsol <file>           input Solution\n"
 "  -O, --outputsol <file>          output Solution\n"
 "  -C, --outsolcfg <file>          output Solution Configuration\n"
 "  -o, --output-solution           output the solutions\n"
 "  -n, --nc4problem <file>         write nc4 problem on file\n"
 "  -D, --dryrun                    skip the compute() call\n"
 "  -v, --verbose[=N]               verbose output (0 = silent, 1 = basic, 2 = debug)\n";

/** @} ---------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @name Utility functions
 *  @{ */

/*--------------------------------------------------------------------------*/

int read_open_netCDF( netCDF::NcFile & f , std::string fn )
{
 fn = resolve_with_prefix( block_prefix , fn );

 try {
  f.open( fn , netCDF::NcFile::read );
  }
 catch( netCDF::exceptions::NcException & e ) {
  std::cerr << exe << ": cannot open nc4 file " << fn << std::endl;
  exit( 1 );
  }

 netCDF::NcGroupAtt gtype = f.getAtt( "SMS++_file_type" );
 if( gtype.isNull() ) {
  std::cerr << exe << ": " << fn << " is not an SMS++ nc4 file" << std::endl;
  exit( 1 );
  }

 int type;
 gtype.getValues( &type );

 if( ( type != eProbFile ) && ( type != eBlockFile ) ) {
  std::cerr << exe << ": " << fn << " is not a valid SMS++ file" << std::endl;
  exit( 1 );
  }

 return( type );
 }

/*--------------------------------------------------------------------------*/

void docopt( void )
{
 // http://docopt.org
 std::cout << docopt_desc << std::endl;
 std::cout << "Usage:" << std::endl
           << "  " << exe << " [options] <file>" << std::endl
           << "  " << exe << " -h | --help" << std::endl << std::endl
           << "Options:"  << std::endl << help << std::endl;
 }

/*--------------------------------------------------------------------------*/

bool process_standard_arg( int opt )
{
 switch( opt ) {
  case 'a': state_out_file = std::string( optarg ); break;
  case 'B': bconf_file = std::string( optarg ); break;
  case 'b': state_in_file = std::string( optarg ); break;
  case 'p' : {
   block_prefix = normalize_prefix( std::string( optarg ) );
   Block::set_filename_prefix( std::string( block_prefix ) );
   break;
  }
  case 'S': sconf_file = std::string( optarg ); break;
  case 'c': conf_prefix = normalize_prefix( std::string( optarg ) );
            break;
  case 'o': output_solution = true; break;
  case 'I': sol_input = std::string( optarg ); break;
  case 'O': sol_output = std::string( optarg ); break;
  case 'C': sol_cfg_file = std::string( optarg ); break;
  case 'n': writeprob = true; break;
  case 'D': dryrun = true; break;
  case 'v': {
   sol_verbose = true;
   verbosity_level = optarg ? std::atoi( optarg ) : 1;
   break;
  }
  case 'h': docopt(); exit( 0 );
  case '?':
  default:  return( false );
  }
 return( true );
 }

/*--------------------------------------------------------------------------*/

void process_args( int argc , char ** argv ,
                   bool ( *custom_arg )( int opt ) )
{
 exe = get_filename( argv[ 0 ] );
 if( argc < 2 ) {
  std::cout << exe << ": no input file" << std::endl
	    << "Try " << exe << "' --help' for more information" << std::endl;
  exit( 1 );
  }

 while( true ) {  // options
  const auto opt = getopt_long( argc , argv , short_opts.data() ,
				long_opts.data() , nullptr );
  if( opt == -1 ) break;

  // tool-specific options are processed first: a tool that re-defines one
  // of the standard letters (e.g. the -n of sddp_solver) means its own
  if( custom_arg && custom_arg( opt ) )  // tool-specific option
   continue;                             // next

  if( process_standard_arg( opt ) )  // if it is a standard one
   continue;                         // next

  std::cout << "Try '" << exe << " --help' for more information"
	    << std::endl;
  exit( 1 );
  }

 if( optind < argc )  // last argument == [Block] filename
  filename = std::string( argv[ optind ] );
 else {
  std::cout << exe << ": no input file" << std::endl
            << "Try '" << exe << " --help' for more information" << std::endl;
  exit( 1 );
  }

 // note: bconf_file, sconf_file and sol_cfg_file are *not* resolved against
 // conf_prefix here: every consumer already resolves them at the point of
 // use (get_config(), output Solution Configuration), so prepending the
 // prefix twice would yield a bogus "config/config/..." path
 }  // end( process_args )

/*--------------------------------------------------------------------------*/

void process_args( int argc , char ** argv )
{
 process_args( argc , argv , nullptr );
 }

/*--------------------------------------------------------------------------*/

void smspp_terminate( void ) {
 std::cerr << "Uncaught exception in executing SMS++:\n";
 try {
  std::rethrow_exception( std::current_exception() );
 }
 catch( const std::exception & e ) {
  std::cerr << "\tException type: " << typeid( e ).name() << "\n";
  std::cerr << "\tException message: " << e.what() << "\n";
 } catch( ... ) {
  std::cerr << "\tUnknown exception" << std::endl;
 }
 std::abort(); // or exit(1)
 }

/*--------------------------------------------------------------------------*/

Block * get_Block( const std::string & b_file )
{
 auto block = Block::deserialize( b_file );
 if( ! block ) {
  std::cerr << "Error: " << b_file << " does not contain a valid Block"
	    << std::endl;
  exit( 1 );
  }

 return( block );
 }

/*--------------------------------------------------------------------------*/

Block * get_Block( const netCDF::NcGroup & group )
{
 auto block = Block::new_Block( group );
 if( ! block ) {
  std::cerr << "Error: group does not contain a valid Block" << std::endl;
  exit( 1 );
  }

 return( block );
 }

/*--------------------------------------------------------------------------*/

Configuration * get_config( const std::string & conf_file )
{
 if( conf_file.empty() )
  return( nullptr );

 auto cfg = Configuration::deserialize(
                            resolve_with_prefix( conf_prefix , conf_file ) );
 return( cfg );
 }

/*--------------------------------------------------------------------------*/

BlockConfig * get_blockconfig( const std::string & conf_file )
{
 auto cfg = get_config( conf_file );
 auto bcfg = dynamic_cast< BlockConfig * >( cfg );
 if( ! bcfg )
  delete cfg;
 return( bcfg );
 }

/*--------------------------------------------------------------------------*/

BlockSolverConfig * get_blocksolverconfig( const std::string & conf_file )
{
 auto cfg = get_config( conf_file );
 auto bscfg = dynamic_cast< BlockSolverConfig * >( cfg );
 if( ! bscfg )
  delete cfg;
 return( bscfg );
 }

/*--------------------------------------------------------------------------*/

void config_Block( Block * block ,
		   Configuration * b_config , Configuration * s_config )
{
 // std::list rather than std::vector since it's built by push_back and
 // only trasversed head-to-tail
 std::list< Block * > BFS;

 if( b_config ) {
  // handle the special case of a "meta" BlockConfig
  if( auto * mb =
      dynamic_cast< SimpleConfiguration< std::map< std::string ,
                                                   Configuration * > >
                                         * >( b_config ) ) {

   // construct the list of all Block inside block
   BFS.push_back( block );
   for( auto bit = BFS.begin() ; bit != BFS.end() ; ++bit )
    for( auto el : ( *bit )->get_nested_Blocks() )
     BFS.push_back( el );

   auto & map = mb->f_value;

   // now BlockConfig-ure all Block whose classname() matches, falling back
   // to the "*" entry (if any) for the non-matching ones
   for( auto b : BFS ) {
    auto bcit = map.find( b->classname() );
    if( bcit == map.end() )
     bcit = map.find( "*" );
    if( bcit != map.end() )
     if( auto bc = dynamic_cast< BlockConfig * >( bcit->second ) ) {
      auto cbc = bc->clone();
      cbc->apply( b );
      delete cbc;
      }
    }
   }
  else  // must be an "ordinary" BlockConfig, just apply() it
   if( auto * bc = dynamic_cast< BlockConfig * >( b_config ) )
    bc->apply( block );
   else
    throw( std::invalid_argument( "config_Block: b_config is not a valid "
				  "[meta]BlockConfig" ) );
  }

 if( s_config ) {
  // handle the special case of a "meta" BlockSolverConfig
  if( auto * mb =
      dynamic_cast< SimpleConfiguration< std::map< std::string ,
                                                   Configuration * > >
                                         * >( s_config ) ) {

   // construct the list of all Block inside block (if not there already)
   if( BFS.empty() ) {
    BFS.push_back( block );
    for( auto bit = BFS.begin() ; bit != BFS.end() ; ++bit )
     for( auto el : ( *bit )->get_nested_Blocks() )
      BFS.push_back( el );
    }

   auto & map = mb->f_value;

   // now BlockSolverConfig-ure all Block whose classname() matches, falling
   // back to the "*" entry (if any) for the non-matching ones, leaf-first
   // (reverse BFS order): a Solver attached to a parent Block (e.g. a
   // LagrangianDualSolver decomposing it) must see the Solvers of its
   // sub-Blocks already in place, so the sub-Blocks are configured first
   for( auto bit = BFS.rbegin() ; bit != BFS.rend() ; ++bit ) {
    auto bscit = map.find( ( *bit )->classname() );
    if( bscit == map.end() )
     bscit = map.find( "*" );
    if( bscit != map.end() )
     if( auto bsc = dynamic_cast< BlockSolverConfig * >( bscit->second ) )
      bsc->apply( *bit );
    }

   // finally, clear() all the BlockSolverConfig for final cleanup
   for( auto & el : map )
    (el.second)->clear();
   }
  else {  // must be an "ordinary" BlockSolverConfig, just apply() it
   if( auto * sc = dynamic_cast< BlockSolverConfig * >( s_config ) ) {
    sc->apply( block );
    sc->clear();
    }
   else
    throw( std::invalid_argument( "config_Block: s_config is not a valid "
				  "[meta]BlockSolverConfig" ) );
   }
  }
 }

/*--------------------------------------------------------------------------*/

void cleanup_bsc( Block * block , Configuration * s_config )
{
 if( ! s_config )
  return;

  // handle the special case of a "meta" BlockSolverConfig
  if( auto * mb =
      dynamic_cast< SimpleConfiguration< std::map< std::string ,
                                                   Configuration * > >
                                         * >( s_config ) ) {
   std::list< Block * > BFS;
   BFS.push_back( block );
   for( auto bit = BFS.begin() ; bit != BFS.end() ; ++bit )
    for( auto el : ( *bit )->get_nested_Blocks() )
     BFS.push_back( el );

   auto & map = mb->f_value;

   // now apply the clear()-ed BlockSolverConfig to all Block whose
   // classname() matches
   for( auto b : BFS )
    if( auto bscit = map.find( b->classname() ); bscit != map.end() )
     if( auto bsc = dynamic_cast< BlockSolverConfig * >( bscit->second ) )
      bsc->apply( b );
   }
  else  // it *must* be a BlockSolverConfig, it has been checked before
   static_cast< BlockSolverConfig * >( s_config )->apply( block );
 }

/*--------------------------------------------------------------------------*/

void get_all( const std::string & b_file , const std::string & bc_file ,
	      const std::string & bsc_file , Block * & block ,
	      Configuration * & s_config )
{
 block = get_Block( b_file );
 auto b_config = get_config( bc_file );
 s_config = get_config( bsc_file );
 config_Block( block , b_config , s_config );
 delete b_config;
 }

/*--------------------------------------------------------------------------*/

void get_all( const netCDF::NcGroup & group , const std::string & bc_file ,
	      const std::string & bsc_file , Block * & block ,
	      Configuration * & s_config )
{
 block = get_Block( group );
 auto b_config = get_config( bc_file );
 s_config = get_config( bsc_file );
 config_Block( block , b_config , s_config );
 delete b_config;
 }

/*--------------------------------------------------------------------------*/
/// get Block, BlockConfig and BlockSolverConfig from group

void get_all( const netCDF::NcGroup & group , Block * & block ,
	      Configuration * & s_config )
{
 // deserialize Block
 block = Block::new_Block( group.getGroup( "Block" ) );
 if( ! block ) {
  std::cerr << "Error: group does not contain a valid Block" << std::endl;
  exit( 1 );
  }

 // deserialize BlockConfig
 auto b_config = BlockConfig::new_Configuration(
					  group.getGroup( "BlockConfig" ) );

 // deserialize BlockSolverConfig
 s_config = BlockSolverConfig::new_Configuration(
					   group.getGroup( "BlockSolver" ) );

 config_Block( block , b_config , s_config );
 delete b_config;
 }

/*--------------------------------------------------------------------------*/

void print_status( int status )
{
 std::cout << "Status = " << status << " (";

 switch( status ) {
  case Solver::kOK:         std::cout << "Success)" << std::endl;    break;
  case Solver::kError:      std::cout << "Error)" << std::endl;      break;
  case Solver::kInfeasible: std::cout << "Infeasible)" << std::endl; break;
  case Solver::kUnbounded:  std::cout << "Unbounded)" << std::endl;  break;
  case Solver::kStopTime:   std::cout << "Stopped for time limit)"
				      << std::endl;                  break;
  case Solver::kStopIter:   std::cout << "Stopped for iteration limit)"
				      << std::endl;                  break;
  default:;
  }
 }

/*--------------------------------------------------------------------------*/

void get_initial_Solution( Block * block )
{
 if( sol_input.empty() )
  return;

 if( auto initsol = Solution::deserialize( sol_input ) ) {
  initsol->write( block );
  delete initsol;
  }
 else
  std::cout << "Warning: input Solution " << sol_input << " invalid"
	    << std::endl;
 }

/*--------------------------------------------------------------------------*/

void get_initial_State( Solver * solver )
{
 if( state_in_file.empty() )
  return;

 try {
  auto state = State::deserialize( state_in_file );
  solver->put_State( *state );
  delete( state );
  }
 catch( netCDF::exceptions::NcException & e ) {
  std::cout << "Warning: State file " << state_in_file
	    << " could not be loaded" << std::endl;
  }
 catch( const std::exception & e ) {
  std::cout << "Warning: error " << e.what()
	    << " occurred while loading the Solver State" << std::endl;
  }
 }

/*--------------------------------------------------------------------------*/

void write_final_Solution( Block * block , Configuration * cfg ,
			   bool replace )
{
 if( sol_output.empty() )
  return;

 // use provided Configuration if any, otherwise (possibly) load one
 Configuration * outsolcfg = cfg;
 if( ( ! outsolcfg ) && ( ! sol_cfg_file.empty() ) )
  if( ! ( outsolcfg = Configuration::deserialize(
          resolve_with_prefix( conf_prefix , sol_cfg_file ) ) ) )
   std::cout << "Warning: output Solution Configuration "
             << sol_cfg_file << " invalid" << std::endl;

 if( auto sol = block->get_Solution( outsolcfg , false ) ) {
  sol->serialize( sol_output , replace );
  delete sol;
  }
 else
  std::cout << "Warning: output Solution empty" << std::endl;

 // if using a "local" Configuration, release it
 if( ! cfg )
  delete outsolcfg;
 }

/*--------------------------------------------------------------------------*/

void write_final_State( Solver * solver , bool replace )
{
 if( state_out_file.empty() )
  return;

 try {
  solver->serialize_State( state_out_file , replace );
  }
 catch( netCDF::exceptions::NcException & e ) {
  std::cout << "Warning: State file " << state_out_file
	    << " could not be opened" << std::endl;
  }
 catch( const std::exception & e ) {
  std::cout << "Warning: error " << e.what()
	    << " occurred while saving the Solver State" << std::endl;
  }
 }

/*--------------------------------------------------------------------------*/

int solve_all( Block * block )
{
 // load initial Solution, if provided - - - - - - - - - - - - - - - - - - - -
 int retval = 0;
 Solution * initsol = nullptr;
 if( ! sol_input.empty() )
  if( ! ( initsol = Solution::deserialize( sol_input ) ) )
   std::cout << "Warning: input Solution " << sol_input << " invalid"
	      << std::endl;

 // prepare file and Configuration for final Solution(s) - - - - - - - - - - -
 Configuration * outsolcfg = nullptr;
 if( ( ! sol_output.empty() ) && ( ! sol_cfg_file.empty() ) )
  if( ! ( outsolcfg = Configuration::deserialize(
          resolve_with_prefix( conf_prefix , sol_cfg_file ) ) ) )
   std::cout << "Warning: output Solution Configuration "
	     << sol_cfg_file << " invalid" << std::endl;

 // for each of the registered Solver- - - - - - - - - - - - - - - - - - - - -
 //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 for( auto solver : block->get_registered_solvers() ) {
  std::cout << "Solver: " << solver->classname() << std::endl;

  if( initsol )  // set the initial Solution, if provided- - - - - - - - - - -
   initsol->write( block );

  // load the initial State, if provided - - - - - - - - - - - - - - - - - - -
  // note: this is bound to fail if there are multiple Solver, since the
  // State is supposed to be Solver-specific, unless all Solver but at
  // most one ignore the State
  get_initial_State( solver );

  if( ! dryrun ) {  // compute() - - - - - - - - - - - - - - - - - - - - - - -
   std::chrono::time_point< std::chrono::system_clock > start , end;
   start = std::chrono::system_clock::now();
   auto status = solver->compute();
   end = std::chrono::system_clock::now();
   std::chrono::duration< double > compute_time = end - start;
   std::cout << "Elapsed time: " << compute_time.count() << " s" << std::endl;

   if( status != Solver::kOK ) retval = 1;

   auto ub = solver->get_ub();
   auto lb = solver->get_lb();
   print_status( status );
   std::cout << "Upper bound = " << ub << std::endl;
   std::cout << "Lower bound = " << lb << std::endl;

   if( ! sol_output.empty() ) {
    if( solver->has_var_solution() )
     solver->get_var_solution();
    else
     std::cout << "Warning: var solution required but not available"
	       << std::endl;

    if( auto cdas = dynamic_cast< CDASolver * >( solver ) )
     if( cdas->has_dual_solution() )
      cdas->get_dual_solution();
    }
   }

  // write final Solution, if required - - - - - - - - - - - - - - - - - - - -
  write_final_Solution( block , outsolcfg );

  // write final State, if required- - - - - - - - - - - - - - - - - - - - - -
  write_final_State( solver );

  }  // end( for( each Solver ) )- - - - - - - - - - - - - - - - - - - - - - -
     //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 delete outsolcfg;
 delete initsol;

 return( retval );

 }  // end( solve_all )

/*--------------------------------------------------------------------------*/

void write_nc4problem( Block * block ,
		       Configuration * b_config , Configuration * s_config )
{
 std::size_t found = filename.find_last_of( '.' );
 std::string nc4_file = filename.substr( 0 , found ) + "_problem.nc4";

 netCDF::NcFile outfile;
 try {
  outfile.open( nc4_file , netCDF::NcFile::replace );
  }
 catch( netCDF::exceptions::NcException & e ) {
  std::cerr << exe << ": cannot open nc4 file " << nc4_file << std::endl;
  exit( 1 );
  }

 outfile.putAtt( "SMS++_file_type" , netCDF::NcInt() , eProbFile );

 block->Block::serialize( outfile , eProbFile );
 netCDF::NcGroup g = outfile.getGroup( "Prob_0" );

 auto new_bc = g.addGroup( "BlockConfig" );
 if( b_config )
  b_config->serialize( new_bc );

 auto new_bsc = g.addGroup( "BlockSolver" );
 if( s_config )
  s_config->serialize( new_bsc );
 outfile.close();
 }

/*--------------------------------------------------------------------------*/
/*------------------------- end common_utils.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
