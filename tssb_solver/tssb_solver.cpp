/*--------------------------------------------------------------------------*/
/*-------------------------- File tssb_solver.cpp --------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * This is a convenient tool for solving a TwoStageStochasticBlock.
 * The description of the TwoStageStochasticBlock must be given in a netCDF file.
 * This tool can be executed as follows:
 *
 *   ./tssb_solver [-s] [-e] [-m NUMBER] [-B FILE] [-S FILE] [-p PATH] [-c PATH]
 *                 <nc4-file>
 *
 * The only mandatory argument is the netCDF file containing the description
 * of the TwoStageStochasticBlock. This netCDF file can be either a BlockFile or
 * a ProbFile. The BlockFile can contain any number of child groups, each one
 * describing an TwoStageStochasticBlock. Every TwoStageStochasticBlock is then
 * solved. The ProbFile can also contain any number of child groups, each one
 * having the description of a TwoStageStochasticBlock alongside the description
 * of a BlockConfig and a BlockSolverConfig for the TwoStageStochasticBlock.
 * Also in this case, every TwoStageStochasticBlock is solved.
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
 * \copyright &copy; by Donato Meoli
 */

#include <getopt.h>
#include <iomanip>
#include <iostream>

#include <BendersBlock.h>
#include <BlockSolverConfig.h>
#include <StochasticBlock.h>
#include <TwoStageStochasticBlock.h>

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/

std::string filename{};
std::string block_config_filename{};
std::string solver_config_filename{};
std::string config_filename_prefix{};
std::string output_solution_directory = ".";
long num_sub_blocks_per_stage = 1;

std::string exe{};         ///< Name of the executable file
std::string docopt_desc{}; ///< Tool description

// Name of Configuration files for each component of the Lagrangian dual of
// the UCBlock
const std::string thermal_config_filename = "TUBSCfg.txt";
const std::string hydro_config_filename = "HSUBSCfg.txt";
const std::string other_unit_config_filename = "OUBSCfg.txt";
const std::string default_config_filename = "LPBSCfg.txt";

/*--------------------------------------------------------------------------*/

// Gets the name of the executable from its full path
std::string get_filename( const std::string & fullpath ) {
 std::size_t found = fullpath.find_last_of( "/\\" );
 return( fullpath.substr( found + 1 ) );
}

/*--------------------------------------------------------------------------*/

void print_help() {
 // http://docopt.org
 std::cout << docopt_desc << std::endl;
 std::cout << "Usage:\n"
           << "  " << exe << " [options] <file>\n"
           << "  " << exe << " -h | --help\n"
           << std::endl
           << "Options:\n"
           << "  -B, --blockcfg <file>           Block configuration.\n"
           << "  -c, --configdir <path>          The prefix for all config filenames.\n"
           << "  -d, --output-dir                Directory where solutions are written.\n"
           << "  -h, --help                      Print this help.\n"
           << "  -p, --prefix <path>             The prefix for all Block filenames.\n"
           << "  -S, --solvercfg <file>          Solver configuration.\n"
           << "  -t, --stage <stage>             Stage from which initial state is taken."
           << std::endl;
}

/*--------------------------------------------------------------------------*/

long get_long_option() {
 char * end = nullptr;
 errno = 0;
 long option = std::strtol( optarg , &end , 10 );
 if( ( ! optarg ) || ( ( option = std::strtol( optarg , &end , 10 ) ) ,
                       ( errno || ( end && *end ) ) ) ) {
  option = -1;
 }
 return( option );
}

/*--------------------------------------------------------------------------*/

void process_args( int argc , char ** argv ) {

 if( argc < 2 ) {
  std::cout << exe << ": no input file\n"
            << "Try " << exe << "' --help' for more information.\n";
  exit( 1 );
 }

 const char * const short_opts = "B:c:d:h:p:sS:";
 const option long_opts[] = {
  { "blockcfg" ,                 required_argument , nullptr , 'B' } ,
  { "configdir" ,                required_argument , nullptr , 'c' } ,
  { "output-dir" ,               required_argument , nullptr , 'd' } ,
  { "help" ,                     no_argument ,       nullptr , 'h' } ,
  { "prefix" ,                   required_argument , nullptr , 'p' } ,
  { "solvercfg" ,                required_argument , nullptr , 'S' } ,
  { nullptr ,                    no_argument ,       nullptr , 0 }
 };

 // Options
 while( true ) {
  const auto opt = getopt_long( argc , argv , short_opts ,
                                long_opts , nullptr );

  if( opt == -1 ) {
   break;
  }

  switch( opt ) {
   case 'B':
    block_config_filename = std::string( optarg );
    break;
   case 'c':
    config_filename_prefix = std::string( optarg );
    Configuration::set_filename_prefix( std::string( optarg ) );
    break;
   case 'd':
    output_solution_directory = std::string( optarg );
    break;
   case 'p':
    Block::set_filename_prefix( std::string( optarg ) );
    break;
   case 'S':
    solver_config_filename = std::string( optarg );
    break;
   case 'h': // -h or --help
    print_help();
    exit( 0 );
   case '?': // Unrecognized option
   default:
    std::cout << "Try " << exe << "' --help' for more information.\n";
    exit( 1 );
  }
 }

 // Last argument
 if( optind < argc ) {
  filename = std::string( argv[ optind ] );
 }
 else {
  std::cout << exe << ": no input file\n"
            << "Try " << exe << "' --help' for more information.\n";
  exit( 1 );
 }
}

/*--------------------------------------------------------------------------*/

void process_prob_file( const netCDF::NcFile & file ) {
 std::multimap< std::string , netCDF::NcGroup > problems = file.getGroups();
 // for each problem descriptor:
 for( auto & problem : problems ) {

  auto & problem_group = problem.second;

  // Deserialize block
  auto block_group = problem_group.getGroup( "Block" );
  auto tss_block = new TwoStageStochasticBlock;
  tss_block->deserialize( block_group );

  // Configure block
  auto block_config_group = problem_group.getGroup( "BlockConfig" );
  auto block_config = static_cast< BlockConfig * >
   ( BlockConfig::new_Configuration( block_config_group ) );
  if( ! block_config )
   throw( std::logic_error( "BlockConfig group was not properly provided." ) );
  block_config->apply( tss_block );
  block_config->clear();

  // Configure solver
  auto solver_config_group = problem_group.getGroup( "BlockSolver" );
  auto block_solver_config = static_cast< BlockSolverConfig * >
   ( BlockSolverConfig::new_Configuration( solver_config_group ) );
  if( ! block_solver_config )
   throw( std::logic_error( "BlockSolver group was not properly provided." ) );
  block_solver_config->apply( tss_block );
  block_solver_config->clear();

  std::cout << "Problem: " << problem.first << std::endl;

  // Solve

  // solve( tss_block );

  // Destroy the Block and the Configurations

  block_config->apply( tss_block );
  delete( block_config );

  block_solver_config->apply( tss_block );
  delete( block_solver_config );

  delete( tss_block );
 }
}

/*--------------------------------------------------------------------------*/

BlockConfig * load_BlockConfig() {

 if( block_config_filename.empty() ) {
  std::cout << "Block configuration was not provided. "
   "Using default configuration." << std::endl;
  return( nullptr );
 }

 std::ifstream block_config_file;
 block_config_file.open( block_config_filename , std::ifstream::in );

 if( ! block_config_file.is_open() ) {
  std::cerr << "Block configuration " + block_config_filename +
   " was not found." << std::endl;
  exit( 1 );
 }

 std::cout << "Using Block configuration in " << block_config_filename
           << "." << std::endl;

 std::string config_name;
 block_config_file >> eatcomments >> config_name;
 auto config = Configuration::new_Configuration( config_name );
 auto block_config = dynamic_cast< BlockConfig * >( config );

 if( ! block_config ) {
  std::cerr << "Block configuration is not valid: "
            << config_name << std::endl;
  delete( config );
  exit( 1 );
 }

 try {
  block_config_file >> *block_config;
 }
 catch( const std::exception& e ) {
  std::cerr << "Block configuration is not valid: " << e.what() << std::endl;
  exit( 1 );
 }

 block_config_file.close();
 return( block_config );
}

/*--------------------------------------------------------------------------*/

BlockSolverConfig * load_BlockSolverConfig( const std::string & filename ) {

 if( filename.empty() ) {
  std::cout << "Solver configuration was not provided. "
   "Using default configuration." << std::endl;
  return( nullptr );
 }

 std::ifstream solver_config_file;
 solver_config_file.open( filename , std::ifstream::in );

 if( ! solver_config_file.is_open() ) {
  std::cerr << "Solver configuration " + filename +
   " was not found." << std::endl;
  exit( 1 );
 }

 std::cout << "Using Solver configuration in " << filename << "." << std::endl;

 std::string config_name;
 solver_config_file >> eatcomments >> config_name;
 auto config = Configuration::new_Configuration( config_name );
 auto solver_config = dynamic_cast< BlockSolverConfig * >( config );

 if( ! solver_config ) {
  std::cerr << "Solver configuration is not valid: " << config_name << std::endl;
  delete( config );
  exit( 1 );
 }

 try {
  solver_config_file >> *solver_config;
 }
 catch( ... ) {
  std::cout << "Solver configuration is not valid." << std::endl;
  exit( 1 );
 }

 solver_config_file.close();
 return( solver_config );
}

/*--------------------------------------------------------------------------*/

void process_block_file( const netCDF::NcFile & file ) {
 std::multimap< std::string , netCDF::NcGroup > blocks = file.getGroups();

 // BlockConfig
 auto given_block_config = load_BlockConfig();

 BlockConfig * block_config = nullptr;
 if( given_block_config ) {
  block_config = given_block_config->clone();
  block_config->clear();
 }

 // BlockSolverConfig
 bool block_solver_config_provided = true;
 auto solver_config = load_BlockSolverConfig( solver_config_filename );
 if( ! solver_config ) {
  std::cerr << "The Solver configuration is not valid." << std::endl;
  exit( 1 );
 }

 auto cleared_solver_config = solver_config->clone();
 cleared_solver_config->clear();

 // For each Block descriptor
 for( auto block_description : blocks ) {

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

  // solve( tss_block );

  // Destroy the TwoStageStochasticBlock and the Configurations

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

 docopt_desc = "SMS++ TSSB solver.\n";
 exe = get_filename( argv[ 0 ] );
 process_args( argc , argv );

 netCDF::NcFile file;
 try {
  file.open( filename , netCDF::NcFile::read );
 } catch( netCDF::exceptions::NcException & e ) {
  std::cerr << "Cannot open nc4 file " << filename << std::endl;
  exit( 1 );
 }

 netCDF::NcGroupAtt gtype = file.getAtt( "SMS++_file_type" );
 if( gtype.isNull() ) {
  std::cerr << filename << " is not an SMS++ nc4 file." << std::endl;
  exit( 1 );
 }

 int type;
 gtype.getValues( &type );

 switch( type ) {
  case eProbFile: {
   std::cout << filename << " is a problem file, "
    "ignoring Block/Solver configurations..." << std::endl;
   process_prob_file( file );
   break;
  }

  case eBlockFile: {
   std::cout << filename << " is a block file." << std::endl;
   process_block_file( file );
   break;
  }

  default:
   std::cerr << filename << " is not a valid SMS++ file." << std::endl;
   exit( 1 );
 }

 file.close();
 return( 0 );
}
