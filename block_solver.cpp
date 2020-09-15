#include <iostream>
#include <getopt.h>

#include <Block.h>
#include <BlockSolverConfig.h>
#include <FRealObjective.h>
#include <RBlockConfig.h>

using namespace SMSpp_di_unipi_it;

std::string filename{};
std::string bconf_file{};
std::string sconf_file{};

void print_help() {
 // http://docopt.org
 std::cout << "Usage: block_solver [options] <nc4-file>" << std::endl
           << std::endl
           << "Options:" << std::endl
           << "  -b <file>, --blockcfg <file>   Block configuration." << std::endl
           << "  -s <file>, --solvercfg <file>  Solver configuration." << std::endl
           << "  -h, --help                     Print this help." << std::endl;
}

void process_args( int argc, char ** argv ) {

 if( argc < 2 ) {
  print_help();
  exit( 1 );
 }

 const char * const short_opts = "b:s:h";
 const option long_opts[] = {
  { "blockcfg",  required_argument, nullptr, 'b' },
  { "solvercfg", required_argument, nullptr, 's' },
  { "help",      no_argument,       nullptr, 'h' },
  { nullptr,     no_argument,       nullptr, 0 }
 };

 // Options
 while( true ) {
  const auto opt = getopt_long( argc, argv, short_opts, long_opts, nullptr );

  if( -1 == opt ) {
   break;
  }

  switch( opt ) {
   case 'b':
    bconf_file = std::string( optarg );
    break;
   case 's':
    sconf_file = std::string( optarg );
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

 switch( type ) {
  case eProbFile: {
   std::cout << filename << " is a problem file, ignoring Block/Solver configurations..." << std::endl;

   std::multimap< std::string, netCDF::NcGroup > problems = f.getGroups();
   // for each problem descriptor:
   for( auto & p : problems ) {

    // Deserialize block
    auto gb = p.second.getGroup( "Block" );
    auto block = Block::new_Block( gb );

    // Configure block
    auto bgc = p.second.getGroup( "BlockConfig" );
    auto b_config = static_cast<BlockConfig *>(BlockConfig::new_Configuration( bgc ));
    if( b_config )
     b_config->apply( block );

    // Configure solver
    auto bgs = p.second.getGroup( "BlockSolver" );
    auto s_config = static_cast<BlockSolverConfig *>(BlockSolverConfig::new_Configuration( bgs ));
    if( s_config )
     s_config->apply( block );

    std::cout << "Problem: " << p.first << std::endl;

    // Solve
    auto solver = block->get_registered_solvers().front();
    if( solver ) {
     auto status = solver->compute();
     auto ub = solver->get_ub();
     auto lb = solver->get_lb();
     std::cout << "Status = " << status << std::endl;
     std::cout << "Upper bound = " << ub << std::endl;
     std::cout << "Lower bound = " << lb << std::endl;
    } else {
     std::cout << "No solvers configured for this problem" << std::endl;
    }
   }
   break;
  }

  case eBlockFile: {
   std::cout << filename << " is a block file" << std::endl;

   std::multimap< std::string, netCDF::NcGroup > blocks = f.getGroups();
   // for each problem descriptor:
   for( auto b : blocks ) {

    // Deserialize block
    auto block = Block::new_Block( b.second );

    // Configure block
    BlockConfig * b_config = nullptr;
    std::ifstream bcf;
    bcf.open( bconf_file, std::ifstream::in );

    if( bcf ) {
     std::cout << "Using Block configuration in " << bconf_file << std::endl;
     std::string config_name;
     bcf >> eatcomments >> config_name;
     b_config = dynamic_cast<BlockConfig *>
      ( Configuration::new_Configuration( config_name ) );
     if( ! b_config ) {
      std::cerr << "Block configuration not valid: " << config_name << std::endl;
      exit( 1 );
     }
     try {
      bcf >> *b_config;
     } catch( const std::exception& e ) {
      std::cerr << "Block configuration not valid: " << e.what() << std::endl;
      exit( 1 );
     }
    } else {
     std::cout << "Block configuration not provided" << std::endl;
    }

    if( b_config )
     b_config->apply( block );

    // Configure solver
    BlockSolverConfig * s_config = nullptr;
    std::ifstream scf;
    scf.open( sconf_file, std::ifstream::in );

    if( scf ) {
     std::cout << "Using Solver configuration in " << sconf_file << std::endl;
     std::string config_name;
     scf >> eatcomments >> config_name;
     s_config = dynamic_cast<BlockSolverConfig *>
      ( Configuration::new_Configuration( config_name ) );
     if( ! s_config ) {
      std::cerr << "Solver configuration not valid: " << config_name << std::endl;
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
    }

    if( s_config )
     s_config->apply( block );

    // Solve
    auto solver = block->get_registered_solvers().front();
    if( solver ) {
     auto status = solver->compute();
     auto ub = solver->get_ub();
     auto lb = solver->get_lb();

     std::cout << "Block: " << b.first << std::endl;
     std::cout << "Status = " << status << std::endl;
     std::cout << "Upper bound = " << ub << std::endl;
     std::cout << "Lower bound = " << lb << std::endl;
    } else {
     std::cout << "No solvers configured for this problem" << std::endl;
    }
   }

   break;
  }
  default:
   std::cerr << filename << " is not a valid SMS++ file" << std::endl;
   exit( 1 );
 }

 return 0;
}
