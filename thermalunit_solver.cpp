#include <iostream>
#include <iomanip>
#include <fstream>
#include <getopt.h>

#include <Block.h>
#include <BlockSolverConfig.h>
#include <RBlockConfig.h>

#include <ThermalUnitBlock.h>

using namespace SMSpp_di_unipi_it;

std::string filename{};
std::string bconf_file{};
std::string sconf_file{};
std::string lp_file{};
std::string nc4_file{};
bool solvVerbose = false;

/*--------------------------------------------------------------------------*/

// Returns a default ThermalUnitBlock configuration
BlockConfig * default_configure_thermalunitblock() {
 auto conf = new BlockConfig();
 conf->f_static_variables_Configuration = new SimpleConfiguration< int >( 15 );
 return conf;
}

/*--------------------------------------------------------------------------*/

// Returns a default Solver configuration
BlockSolverConfig * default_configure_solver() {
 auto s_config = new BlockSolverConfig;
 auto c_config = new ComputeConfig;

 if( solvVerbose ) {
  c_config->set_par( "intLogVerb", 1 );
 }

 s_config->add_ComputeConfig( "CPXMILPSolver", c_config );
 return s_config;
}

/*--------------------------------------------------------------------------*/

void print_help() {
 // http://docopt.org
 std::cout << "SMS++ thermal unit solver.\n"
           << std::endl
           << "Usage: thermalunit_solver [options] <nc4-file>\n"
           << std::endl
           << "Options:\n"
           << "  -B <file>, --blockcfg <file>    Block configuration.\n"
           << "  -S <file>, --solvercfg <file>   Solver configuration.\n"
           << "  -w <file>, --writelp <file>     Write LP problem on file.\n"
           << "  -n <file>, --nc4problem <file>  Write nc4 problem on file.\n"
           << "  -v, --verbose                   Make the solver verbose.\n"
           << "  -h, --help                      Print this help.\n";
}

/*--------------------------------------------------------------------------*/

void process_args( int argc, char ** argv ) {

 if( argc < 2 ) {
  std::cout << "thermalunit_solver: no input file\n"
            << "Try thermalunit_solver --help' for more information.\n";
  exit( 1 );
 }

 const char * const short_opts = "B:S:w:n:vh";
 const option long_opts[] = {
  { "blockcfg",   required_argument, nullptr, 'B' },
  { "solvercfg",  required_argument, nullptr, 'S' },
  { "writelp",    required_argument, nullptr, 'w' },
  { "nc4problem", required_argument, nullptr, 'n' },
  { "verbose",    no_argument,       nullptr, 'v' },
  { "help",       no_argument,       nullptr, 'h' },
  { nullptr,      no_argument,       nullptr, 0 }
 };

 // Options
 while( true ) {
  const auto opt = getopt_long( argc, argv, short_opts, long_opts, nullptr );

  if( -1 == opt ) {
   break;
  }

  switch( opt ) {
   case 'B':
    bconf_file = std::string( optarg );
    break;
   case 'S':
    sconf_file = std::string( optarg );
    break;
   case 'w':
    lp_file = std::string( optarg );
    break;
   case 'n':
    nc4_file = std::string( optarg );
    break;
   case 'v':
    solvVerbose = true;
    break;
   case 'h':
    print_help();
    exit( 0 );
   case '?':
   default:
    std::cout << "Try thermalunit_solver --help' for more information.\n";
    exit( 1 );
  }
 }

 // Last argument
 if( optind < argc ) {
  filename = std::string( argv[ optind ] );
 } else {
  std::cout << "thermalunit_solver: no input file\n"
            << "Try thermalunit_solver --help' for more information.\n";
  exit( 1 );
 }
}

/*--------------------------------------------------------------------------*/

void print_status( int status ) {
 std::cout << "Status = " << status << " (";

 switch( status ) {
  case Solver::kOK:
   std::cout << "Success)" << std::endl;
   break;
  case Solver::kError:
   std::cout << "Error)" << std::endl;
   break;
  case Solver::kInfeasible:
   std::cout << "Infeasible)" << std::endl;
   break;
  case Solver::kUnbounded:
   std::cout << "Unbounded)" << std::endl;
   break;
  case Solver::kStopTime:
   std::cout << "Stopped for time limit)" << std::endl;
   break;
  case Solver::kStopIter:
   std::cout << "Stopped for iteration limit)" << std::endl;
   break;
  default:;
 }
}

/*--------------------------------------------------------------------------*/

void solve_all( Block * block ) {
 for( auto solver : block->get_registered_solvers() ) {
  std::cout << "Solver: " << solver->classname() << std::endl;
  auto status = solver->compute();
  auto ub = solver->get_ub();
  auto lb = solver->get_lb();
  print_status( status );
  std::cout << "Upper bound = " << ub << std::endl;
  std::cout << "Lower bound = " << lb << std::endl;
 }
}

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc, char ** argv ) {

 process_args( argc, argv );

 netCDF::NcFile f;
 try {
  f.open( filename, netCDF::NcFile::read );
 } catch( netCDF::exceptions::NcException & e ) {
  std::cerr << "thermalunit_solver: "
            << "cannot open nc4 file " << filename << std::endl;
  exit( 1 );
 }

 netCDF::NcGroupAtt gtype = f.getAtt( "SMS++_file_type" );
 if( gtype.isNull() ) {
  std::cerr << "thermalunit_solver: "
            << filename << " is not an SMS++ nc4 file" << std::endl;
  exit( 1 );
 }

 int type;
 gtype.getValues( &type );

 if( type != eBlockFile ) {
  std::cerr << "thermalunit_solver: "
            << filename << " is not an SMS++ nc4 Block file" << std::endl;;
  exit( 1 );
 }

 netCDF::NcGroup bg = f.getGroup( "Block_0" );
 if( bg.isNull() ) {
  std::cerr << "thermalunit_solver: "
            << "Block_0 empty or undefined in " << filename << std::endl;
  exit( 1 );
 }

 // Deserialize block
 auto block = dynamic_cast<ThermalUnitBlock *>(Block::new_Block( "ThermalUnitBlock" ));
 block->deserialize( bg );

 // Configure block
 BlockConfig * b_config = nullptr;
 std::ifstream bcf;
 bcf.open( bconf_file, std::ifstream::in );

 if( bcf.is_open() ) {
  std::cout << "Using Block configuration in " << bconf_file << std::endl;
  std::string config_name;
  bcf >> eatcomments >> config_name;
  b_config = dynamic_cast<BlockConfig *>
  ( Configuration::new_Configuration( config_name ) );

  if( !b_config ) {
   std::cerr << "thermalunit_solver: "
             << "Block configuration not valid: " << config_name
             << std::endl;
  }

  try {
   bcf >> *b_config;
  } catch( const std::exception & e ) {
   std::cerr << "thermalunit_solver: "
             << "Block configuration not valid: " << e.what() << std::endl;
   exit( 1 );
  }

 } else {
  std::cout << "Block configuration not provided" << std::endl;
  b_config = default_configure_thermalunitblock();
 }

 if( b_config ) {
  b_config->apply( block );
 }

 // Configure solver
 BlockSolverConfig * s_config = nullptr;
 std::ifstream scf;
 scf.open( sconf_file, std::ifstream::in );

 if( scf.is_open() ) {
  std::cout << "Using Solver configuration in " << sconf_file << std::endl;
  std::string config_name;
  scf >> eatcomments >> config_name;
  s_config = dynamic_cast<BlockSolverConfig *>
  ( Configuration::new_Configuration( config_name ) );

  if( !s_config ) {
   std::cerr << "Solver configuration not valid: " << config_name
             << std::endl;
   exit( 1 );
  }

  try {
   scf >> *s_config;
  } catch( ... ) {
   std::cout << "Solver configuration not valid" << std::endl;
   exit( 1 );
  }
 } else {
  std::cout << "Solver configuration not provided" << std::endl;
  s_config = default_configure_solver();
 }

 if( s_config ) {
  s_config->apply( block );
 }

 // Solve
 std::cout.setf( std::ios::scientific, std::ios::floatfield );
 std::cout << std::setprecision( 8 );
 solve_all( block );

 // Write nc4 problem file
 if( !nc4_file.empty() ) {
  netCDF::NcFile outfile;
  outfile.open( nc4_file, netCDF::NcFile::replace );
  outfile.putAtt( "SMS++_file_type", netCDF::NcInt(), eProbFile );

  block->Block::serialize( outfile, eProbFile );
  netCDF::NcGroup g = outfile.getGroup( "Prob_0" );

  auto new_bc = g.addGroup( "BlockConfig" );
  if( b_config ) {
   b_config->serialize( new_bc );
  }

  auto new_bsc = g.addGroup( "BlockSolver" );
  if( s_config ) {
   s_config->serialize( new_bsc );
  }

  outfile.close();
 }

 return 0;
}
