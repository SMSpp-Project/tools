#include <iostream>
#include <getopt.h>

#include <AbstractBlock.h>
#include <BlockSolverConfig.h>
#include <ThermalUnitBlock.h>

using namespace SMSpp_di_unipi_it;

std::string filename{};
std::string lp_file{};
std::string solver_name{};
std::string nc4_problem{};

void print_help() {
 // http://docopt.org
 std::cout << "Usage: thermalunit_solver [options] <nc4-file>" << std::endl
           << std::endl
           << "Options:" << std::endl
           << "  -s <solver>, --solver <solver>  Choose solver." << std::endl
           << "                                  Available solvers are: cplex, dp." << std::endl
           << "  -w <file>, --writelp <file>     Write LP problem on file." << std::endl
           << "  -n <file>, --nc4problem <file>  Write nc4 problem on file." << std::endl
           << "  -h, --help                      Print this help." << std::endl;
}

void process_args( int argc, char ** argv ) {

 if( argc < 2 ) {
  print_help();
  exit( 1 );
 }

 const char * const short_opts = "s:w:n:h";
 const option long_opts[] = {
  { "solver",     required_argument, nullptr, 's' },
  { "writelp",    required_argument, nullptr, 'w' },
  { "nc4problem", required_argument, nullptr, 'n' },
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
   case 's':
    solver_name = std::string( optarg );
   case 'w':
    lp_file = std::string( optarg );
    break;
   case 'n':
    nc4_problem = std::string( optarg );
    break;
   case 'h': // -h or --help
    print_help();
    exit( 0 );
   case '?': // Unrecognized option
   default:
    print_help();
    exit( 1 );
  }
 }

 // Last argument
 if( optind < argc ) {
  filename = std::string( argv[ optind ] );
 } else {
  print_help();
  exit( 1 );
 }
}

int main( int argc, char ** argv ) {

 solver_name = "cplex";
 process_args( argc, argv );

 netCDF::NcFile f;
 try {
  f.open( filename, netCDF::NcFile::read );
 } catch( netCDF::exceptions::NcException & e ) {
  std::cerr << "Cannot open nc4 file " << filename << std::endl;
  exit( 1 );
 }

 netCDF::NcGroupAtt gtype = f.getAtt( "SMS++_file_type" );
 if( gtype.isNull() ) {
  std::cerr << filename << " is not an SMS++ nc4 file" << std::endl;
  exit( 1 );
 }

 int type;
 gtype.getValues( &type );

 if( type != eBlockFile ) {
  std::cerr << filename << " is not an SMS++ nc4 Block file" << std::endl;
  exit( 1 );
 }

 netCDF::NcGroup bg = f.getGroup( "Block_0" );
 if( bg.isNull() ) {
  std::cerr << "Block_0 empty or undefined in " << filename << std::endl;
  exit( 1 );
 }

 // Deserialize block
 auto tub = dynamic_cast<ThermalUnitBlock *>(Block::new_Block( "ThermalUnitBlock" ));
 tub->deserialize( bg );

 // Configure block
 auto conf = new BlockConfig();
 conf->f_static_variables_Configuration = new SimpleConfiguration< int >( 15 );
 conf->apply( tub );

 // Configure solver
 auto slv_conf = new BlockSolverConfig();

 if( solver_name == "cplex" ) {
  auto cplex_config = new ComputeConfig;
  cplex_config->set_par( "dblRAccSol" , 1.0e-8 );
  slv_conf->add_ComputeConfig( "CPXMILPSolver" , cplex_config );
 } else if( solver_name == "dp" ) {
  slv_conf->add_ComputeConfig( "ThermalUnitDPSolver" );
 } else {
  std::cerr << "Available solvers are: cplex, dp" << std::endl;
  exit( 1 );
 }

 slv_conf->apply( tub );
 auto solver = tub->get_registered_solvers().front();

 // Solve
 int status = solver->compute();
 solver->get_var_solution();
 auto ub = solver->get_ub();
 auto lb = solver->get_lb();

 // Write nc4 problem file
 if( !nc4_problem.empty() ) {
  netCDF::NcFile outfile;
  outfile.open( "test.nc4", netCDF::NcFile::replace );
  outfile.putAtt( "SMS++_file_type", netCDF::NcInt(), eProbFile );

  tub->Block::serialize( outfile, eProbFile );
  netCDF::NcGroup g = outfile.getGroup( "Prob_0" );

  auto new_bc = g.addGroup( "BlockConfig" );
  conf->serialize( new_bc );

  auto new_bsc = g.addGroup( "BlockSolver" );
  slv_conf->serialize( new_bsc );

  outfile.close();
 }

 // std::ofstream of1("blockconfig.txt");
 // std::ofstream of2("solverconfig.txt");
 //
 // of1 << *tub->get_BlockConfig();
 // of2 << *tub->get_SolverConfig();
 //
 // of1.close();
 // of2.close();

 std::cout << "Status = " << status << std::endl;
 std::cout << "Upper bound = " << ub << std::endl;
 std::cout << "Lower bound = " << lb << std::endl;

 return 0;
}
