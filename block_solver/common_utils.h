/*--------------------------------------------------------------------------*/
/*--------------------------- common_utils.h -------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Some common utilities for SMS++ tools.
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

#include <getopt.h>  // for getting command line parameters
#include <chrono>    // for measuring compute time

#include <Block.h>
#include <BlockSolverConfig.h>
#include <CDASolver.h>
#include <Solution.h>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

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

/// default short command-line options
std::string short_opts = "a:B:b:p:S:c:on:I:O:C:Dvh";

/// default long command-line options
std::vector< option > long_opts = {
 { "help"            , no_argument ,       nullptr , 'h' } ,
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
 { "dryrun"          , no_argument ,       nullptr , 'D' } ,
 { "verbose"         , no_argument ,       nullptr , 'v' } ,
 { nullptr           , no_argument ,       nullptr , 0 }
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
 "  -v, --verbose                   make the Solver verbose\n";

/** @} ---------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @name Utility functions 
 *  @{ */

/// gets the name of the executable from its full path

std::string get_filename( const std::string & fullpath )
{
 std::size_t found = fullpath.find_last_of( "/\\" );
 return( fullpath.substr( found + 1 ) );
 }

/*--------------------------------------------------------------------------*/
/// gets an option as a string, converts it to long

long get_long_option( char * end = nullptr )
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
/// open a netCDF file for reeading, returns its type

int read_open_netCDF( netCDF::NcFile & f , std::string fn )
{
 if( ! block_prefix.empty() )
  fn.insert( 0 , block_prefix );

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
/// prints the tool description and usage

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
/// processes any one of the default command-line arguments

bool process_standard_arg( int opt )
{
 switch( opt ) {
  case 'a': state_out_file = std::string( optarg ); break;
  case 'B': bconf_file = std::string( optarg ); break;
  case 'b': state_in_file = std::string( optarg ); break;
  case 'p': block_prefix = std::string( optarg );
            Block::set_filename_prefix( std::string( block_prefix ) );
	    break;
  case 'S': sconf_file = std::string( optarg ); break;
  case 'c': conf_prefix = std::string( optarg );
            Configuration::set_filename_prefix( std::string( conf_prefix ) );
	    break;
  case 'o': output_solution = true;
  case 'I': sol_input = std::string( optarg ); break;
  case 'O': sol_output = std::string( optarg ); break;
  case 'C': sol_cfg_file = std::string( optarg ); break;
  case 'n': writeprob = true; break;
  case 'D': dryrun = true; break;
  case 'v': sol_verbose = true; break;
  case 'h': docopt(); exit( 0 );
  case '?':
  default:  return( false );
  }
 return( true );
 }

/*--------------------------------------------------------------------------*/
/// processes all default command-line arguments

void process_args( int argc , char ** argv )
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

  if( ! process_standard_arg( opt ) ) {
   std::cout << "Try '" << exe << " --help' for more information"
	     << std::endl;
   exit( 1 );
   }
  }

 if( optind < argc )  // last argument == [Block] filename
  filename = std::string( argv[ optind ] );
 else {
  std::cout << exe << ": no input file" << std::endl
            << "Try '" << exe << " --help' for more information" << std::endl;
  exit( 1 );
  }
 }  // end( process_args )

/*--------------------------------------------------------------------------*/
/// gets a BlockConfig from a BlockConfig file

BlockConfig * get_blockconfig( const std::string & conf_file )
{
 if( conf_file.empty() )
  return( nullptr );

 auto cfg = Configuration::deserialize( conf_file );
 auto bcfg = dynamic_cast< BlockConfig * >( cfg );
 if( ! bcfg )
  delete cfg;
 return( bcfg );
 }

/*--------------------------------------------------------------------------*/
/// gets a BlockSolverConfig from a BlockSolverConfig file

BlockSolverConfig * get_blocksolverconfig( const std::string & conf_file )
{
 if( conf_file.empty() )
  return( nullptr );

 auto cfg = Configuration::deserialize( conf_file );
 auto bscfg = dynamic_cast< BlockSolverConfig * >( cfg );
 if( ! bscfg )
  delete cfg;
 return( bscfg );
 }

/*--------------------------------------------------------------------------*/
/// BlockConfig-ure and BlockSolverConfig-ure a Block

void config_Block( Block * block , BlockConfig * b_config ,
		   BlockSolverConfig * s_config )
{
 if( b_config )
  b_config->apply( block );
 
 if( s_config ) {
  s_config->apply( block );
  s_config->clear();
  }
 }

/*--------------------------------------------------------------------------*/
/// get Block, BlockConfig and BlockSolverConfig from files, configure all

void get_all( const std::string & b_file , const std::string & bc_file ,
	      const std::string & bsc_file , Block * & block ,
	      BlockConfig * & b_config , BlockSolverConfig * & s_config )
{
 block = Block::deserialize( b_file );
 if( ! block ) {
  std::cerr << "Error: " << b_file << " does not contain a valid Block"
	    << std::endl;
  exit( 1 );
  }
 
 b_config = get_blockconfig( bc_file );
 s_config = get_blocksolverconfig( bsc_file );
 config_Block( block , b_config , s_config );
 }

/*--------------------------------------------------------------------------*/
/// get Block from group, BlockConfig and BlockSolverConfig from files

void get_all( const netCDF::NcGroup & group , const std::string & bc_file ,
	      const std::string & bsc_file , Block * & block ,
	      BlockConfig * & b_config , BlockSolverConfig * & s_config )
{
 block = Block::new_Block( group );
 if( ! block ) {
  std::cerr << "Error: group does not contain a valid Block" << std::endl;
  exit( 1 );
  }
 
 b_config = get_blockconfig( bc_file );
 s_config = get_blocksolverconfig( bsc_file );
 config_Block( block , b_config , s_config );
 }

/*--------------------------------------------------------------------------*/
/// get Block, BlockConfig and BlockSolverConfig from group

void get_all( const netCDF::NcGroup & group , Block * & block ,
	      BlockConfig * & b_config , BlockSolverConfig * & s_config )
{
 // deserialize Block
 auto gb = group.getGroup( "Block" );
 block = Block::new_Block( gb );
 if( ! block ) {
  std::cerr << "Error: group does not contain a valid Block" << std::endl;
  exit( 1 );
  }

 // deserialize BlockConfig
 auto bgc = group.getGroup( "BlockConfig" );
 auto c = BlockConfig::new_Configuration( bgc );
 if( auto bc = dynamic_cast< BlockConfig * >( c ) )
  b_config = bc;
 else
  delete c;

 // deserialize BlockSolverConfig
 auto bgs = group.getGroup( "BlockSolver" );
 c = BlockSolverConfig::new_Configuration( bgs );
 if( auto bsc = dynamic_cast< BlockSolverConfig * >( c ) )
  s_config = bsc;
 else
  delete c;

 config_Block( block , b_config , s_config );
 }

/*--------------------------------------------------------------------------*/
/// prints the status in a human-readable form

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
/// get and set the initial Solution

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
/// get and set the initial State

void get_initial_State( Solver * solver )
{
 if( state_in_file.empty() )
  return;

 try {
  auto state = State::new_State( state_in_file );
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
/// write the final Solution, using given Configuration if provided
/** Write the Solution currently in the given \p block, using given
 * Configuration \p cfg (if provided, default not) to produce it; bu default
 * append to the file with filename sol_output, rather than replacing it. */

void write_final_Solution( Block * block , Configuration * cfg = nullptr ,
			   bool replace = false  )
{
 if( sol_output.empty() )
  return;

 // use provided Configuration if any, otherwise (possibly) load one
 Configuration * outsolcfg = cfg;
 if( ( ! outsolcfg ) && ( ! sol_cfg_file.empty() ) )
  if( ! ( outsolcfg = Configuration::deserialize( sol_cfg_file ) ) )
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
/// write the final State, by default appending rather than replacing

void write_final_State( Solver * solver , bool replace = false )
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
/// compute() the Block with all available Solver(s) (unless dry run)

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
  if( ! ( outsolcfg = Configuration::deserialize( sol_cfg_file ) ) )
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

   if ( status != Solver::kOK ) retval = 1;

   auto ub = solver->get_ub();
   auto lb = solver->get_lb();
   print_status( status );
   std::cout << "Upper bound = " << ub << std::endl;
   std::cout << "Lower bound = " << lb << std::endl;

   if( ! sol_output.empty() ) {
    solver->get_var_solution();
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
/// writes a new nc4 problem using the Block and its Configuration(s)

void write_nc4problem( Block * block , BlockConfig * b_config ,
                       BlockSolverConfig * s_config )
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

/** @} ---------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

#endif  //__COMMON_UTILS

/*--------------------------------------------------------------------------*/
/*------------------------- end common_utils.h -----------------------------*/
/*--------------------------------------------------------------------------*/
