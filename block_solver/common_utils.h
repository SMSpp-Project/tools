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

#ifndef NDEBUG
 #include <queue>    // For scanning the sub-Blocks
 #include <FRealObjective.h>
#endif

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/
/** @name Global variables used by every tool
 *  @{ */

std::string filename {};      ///< input filename
std::string bconf_file {};    ///< BlockConfig filename
std::string sconf_file {};    ///< BlockSolverConfig filename
std::string exe {};           ///< name of the executable file
std::string docopt_desc {};   ///< tool description
std::string sol_input {};     ///< filename of input Solution
std::string sol_output {};    ///< filename of output Solution 
std::string sol_cfg_file {};  ///< filename of output Solution Configuration

bool solvVerbose = false;     ///< if the solver should be verbose
bool writeprob = false;       ///< if the problem should be written back
bool dryrun = false;          ///< if compute() need not really ba called

int solution_output_type = 1;
/**< This indicates if and how a solution of the problem is output. If
 *
 * - solution_output_type = 0, then no solution is output;
 *
 * - solution_output_type = 1, then the solution is output to the screen;
 *
 * - solution_output_type = 2, then the solution is output to file(s);
 *
 * - solution_output_type = 3, then the solution is output to both the screen
 *   and file(s);
 */

/** @} ---------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/
/// gets the name of the executable from its full path

std::string get_filename( const std::string & fullpath )
{
 std::size_t found = fullpath.find_last_of( "/\\" );
 return( fullpath.substr( found + 1 ) );
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
           << "Options:" << std::endl
           << "  -B, --blockcfg <file>    Block configuration" << std::endl
           << "  -p, --prefix <path>      the prefix for all Block filenames"
	   << std::endl
           << "  -S, --solvercfg <file>   Solver configuration" << std::endl
           << "  -c, --configdir <path>   the prefix for all config filenames"
	   << std::endl
           << "  -I, --inputsol <file>    input Solution" << std::endl
           << "  -O, --outputsol <file>   output Solution" << std::endl
           << "  -t, --outsolcfg <file>   output Solution Configuration"
	   << std::endl
           << "  -n, --nc4problem <file>  write nc4 problem on file"
	   << std::endl
           << "  -d, --dryrun             if the compute() call si skipped"
	   << std::endl
           << "  -v, --verbose            make the solver verbose"
	   << std::endl
           << "  -o, --output <type>      solution output type [1]"
	   << std::endl
	   << "                           (0 none, 1 screen, 2 files, 3 both)"
	   << std::endl
           << "  -h, --help               Print this help.\n";
 }

/*--------------------------------------------------------------------------*/
/// processes the command line arguments

void process_args( int argc , char ** argv )
{
 if( argc < 2 ) {
  std::cout << exe << ": no input file" << std::endl
	    << "Try " << exe << "' --help' for more information" << std::endl;
  exit( 1 );
  }

 const char * const short_opts = "B:p:S:c:onIOtdvh";
 const option long_opts[] = {
  { "blockcfg" ,   required_argument , nullptr , 'B' } ,
  { "prefix" ,     required_argument , nullptr , 'p' } ,
  { "solvercfg" ,  required_argument , nullptr , 'S' } ,
  { "configdir" ,  required_argument , nullptr , 'c' } ,
  { "output" ,     no_argument ,       nullptr , 'o' } ,
  { "nc4problem" , no_argument ,       nullptr , 'n' } ,
  { "inputsol" ,   no_argument ,       nullptr , 'I' } ,
  { "outputsol" ,  no_argument ,       nullptr , 'O' } ,
  { "outsolcfg" ,  no_argument ,       nullptr , 't' } ,
  { "dryrun" ,     no_argument ,       nullptr , 'd' } ,
  { "verbose" ,    no_argument ,       nullptr , 'v' } ,
  { "help" ,       no_argument ,       nullptr , 'h' } ,
  { nullptr ,      no_argument ,       nullptr , 0 }
  };

 // options
 while( true ) {
  const auto opt = getopt_long( argc , argv , short_opts , long_opts ,
				nullptr );
  if( -1 == opt ) break;

  switch( opt ) {
   case 'B': bconf_file = std::string( optarg ); break;
   case 'p': Block::set_filename_prefix( std::string( optarg ) ); break;
   case 'S': sconf_file = std::string( optarg ); break;
   case 'c': Configuration::set_filename_prefix( std::string( optarg ) );
             break;
   case 'o': { auto s = std::string( optarg );
	       if( s.size() != 1 || s.front() < '0' || s.front() > '3' ) {
		std::cout << "Invalid output solution type " << s << std::endl
			  << "Try " << exe << "' --help' for more information"
			  << std::endl;
		exit( 1 );
	        }
	       solution_output_type = s.front() - '0';
	       break;
               }
   case 'I': sol_input = std::string( optarg ); break;
   case 'O': sol_output = std::string( optarg ); break;
   case 't': sol_cfg_file = std::string( optarg ); break;
   case 'n': writeprob = true; break;
   case 'd': dryrun = true; break;
   case 'v': solvVerbose = true; break;
   case 'h': docopt(); exit( 0 );
   case '?':
   default:  std::cout << "Try " << exe << "' --help' for more information"
		       << std::endl;
             exit( 1 );
   }
  }

 // last argument
 if( optind < argc )
  filename = std::string( argv[ optind ] );
 else {
  std::cout << exe << ": no input file" << std::endl
            << "Try " << exe << "' --help' for more information" << std::endl;
  exit( 1 );
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

void get_all( netCDF::NcGroup group , const std::string & bc_file ,
	      const std::string & bsc_file , Block * & block ,
	      BlockConfig * & b_config , BlockSolverConfig * & s_config )
{
 auto block = Block::new_Block( group );
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

void get_all( netCDF::NcGroup group , Block * & block ,
	      BlockConfig * & b_config , BlockSolverConfig * & s_config )
{
 // deserialize block
 auto gb = group.getGroup( "Block" );
 auto block = Block::new_Block( gb );
 if( ! block ) {
  std::cerr << "Error: group does not contain a valid Block" << std::endl;
  exit( 1 );
  }

 // Configure block
 auto bgc = group.getGroup( "BlockConfig" );
 auto b_config = dynamic_cast< BlockConfig * >(
				     BlockConfig::new_Configuration( bgc ) );

 // Configure solver
 auto bgs = group.getGroup( "BlockSolver" );
 auto s_config = static_cast< BlockSolverConfig * >(
			       BlockSolverConfig::new_Configuration( bgs ) );

 config_Block( block , b_config , s_config );
 }

/*--------------------------------------------------------------------------*/
/// gets a BlockConfig from a BlockConfig file

BlockConfig * get_blockconfig( const std::string & conf_file )
{
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
/// solves the problem with all available solvers (unless dry run)

int solve_all( Block * block )
{
 int retval = 0;
 Solution * initsol = nullptr;
 if( ! sol_input.empty() )
  initsol = Solution::deserialize( sol_input );

 netCDF::NcFile f;
 Configuration * outsolcfg = nullptr;
 if( ! sol_output.empty() ) {
  f.open( sol_output , netCDF::NcFile::write );
  if( ! sol_cfg_file.empty() )
   outsolcfg = Configuration::deserialize( sol_cfg_file );
  }

 // for each of the registered Solver
 for( auto solver : block->get_registered_solvers() ) {
  std::cout << "Solver: " << solver->classname() << std::endl;

  if( initsol )
   sol->write( block );

  if( ! dryrun ) {
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
     cdas->get_dual_solution();
    }
   }

  if( ! sol_output.empty() )
   if( auto sol = block->get_Solution( outsolcfg , false ) ) {
    sol->serialize( f );
    delete sol;
    }

  }  // end( for( each Solver ) )
 
 delete outsolcfg;
 delete initsol;

 return( retval );
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
/// writes a new nc4 problem using the block and its configurations

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

/*--------------------------------------------------------------------------*/

#endif  //__COMMON_UTILS

/*--------------------------------------------------------------------------*/
/*------------------------- end common_utils.h -----------------------------*/
/*--------------------------------------------------------------------------*/
