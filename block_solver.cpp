#include <iostream>
#include <iomanip>
#include <fstream>
#include <getopt.h>

#include <Block.h>
#include <BlockSolverConfig.h>
#include <RBlockConfig.h>

#include <UnitBlock.h>
#include <HydroSystemUnitBlock.h>

using namespace SMSpp_di_unipi_it;

std::string filename{};
std::string bconf_file{};
std::string sconf_file{};
bool solvVerbose = false;

/*--------------------------------------------------------------------------*/

// std::string get_library_name( const std::string & class_name ) {
//  const std::map< std::string, std::string > class_to_lib{
//   { "ThermalUnitBlock", "UCBlock" },
//   { "UCBlock",          "UCBlock" },
//  };
//
//  return class_to_lib.at( class_name );
// }

/*--------------------------------------------------------------------------*/

// Returns a default UCBlock configuration
BlockConfig * default_configure_ucblock( Block * uc_block ) {

 BlockConfig * b_config = new RBlockConfig;
 auto num_nested_blocks = uc_block->get_number_nested_Blocks();
 for( Block::Index i = 0; i < num_nested_blocks; ++i ) {

  auto sub_block = uc_block->get_nested_Block( i );
  if( !dynamic_cast<UnitBlock *>( sub_block ) )
   continue;

  auto subconf = new RBlockConfig;
  subconf->f_static_variables_Configuration =
   new SimpleConfiguration< int >( 15 );

  auto hu_block = dynamic_cast<HydroSystemUnitBlock *>( sub_block );

  if( hu_block != nullptr ) {
   auto num_nested_blocks_hydro = hu_block->get_number_nested_Blocks();
   for( Block::Index j = 0; j < num_nested_blocks_hydro; ++j ) {
    auto sub_Block_hydro = hu_block->get_nested_Block( j );
    auto sub_pf_block =
     dynamic_cast<PolyhedralFunctionBlock *>( sub_Block_hydro );

    if( sub_pf_block != nullptr ) {
     auto subsubconf = new BlockConfig();
     subsubconf->f_static_variables_Configuration =
      new SimpleConfiguration< int >( 1 );
     subconf->add_sub_BlockConfig( subsubconf, j );
    }
   }
  }

  static_cast<RBlockConfig *>( b_config )->add_sub_BlockConfig( subconf, i );
 }

 return b_config;
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
 std::cout << "SMS++ generic block and problem solver.\n"
           << std::endl
           << "Usage: block_solver [options] <nc4-file>\n"
           << std::endl
           << "Options:\n"
           << "  -B <file>, --blockcfg <file>   Block configuration.\n"
           << "  -S <file>, --solvercfg <file>  Solver configuration.\n"
           << "  -v, --verbose                  Make the solver verbose.\n"
           << "  -h, --help                     Print this help.\n";
}

/*--------------------------------------------------------------------------*/

void process_args( int argc, char ** argv ) {

 if( argc < 2 ) {
  std::cout << "block_solver: no input file\n"
            << "Try block_solver --help' for more information.\n";
  exit( 1 );
 }

 const char * const short_opts = "B:S:vh";
 const option long_opts[] = {
  { "blockcfg",  required_argument, nullptr, 'B' },
  { "solvercfg", required_argument, nullptr, 'S' },
  { "verbose",   no_argument,       nullptr, 'v' },
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
   case 'B':
    bconf_file = std::string( optarg );
    break;
   case 'S':
    sconf_file = std::string( optarg );
    break;
   case 'v':
    solvVerbose = true;
    break;
   case 'h':
    print_help();
    exit( 0 );
   case '?':
   default:
    std::cout << "Try block_solver --help' for more information.\n";
    exit( 1 );
  }
 }

 // Last argument
 if( optind < argc ) {
  filename = std::string( argv[ optind ] );
 } else {
  std::cout << "block_solver: no input file\n"
            << "Try block_solver --help' for more information.\n";
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
  std::cerr << "block_solver: "
            << "cannot open nc4 file " << filename << std::endl;
  exit( 1 );
 }

 netCDF::NcGroupAtt gtype = f.getAtt( "SMS++_file_type" );
 if( gtype.isNull() ) {
  std::cerr << "block_solver: "
            << filename << " is not an SMS++ nc4 file" << std::endl;
  exit( 1 );
 }

 int type;
 gtype.getValues( &type );

 switch( type ) {

  case eProbFile: {
   // Problem file containing one or more Block/BlockConfig/BlockSolver sets

   std::cout << filename
             << " is a problem file, ignoring Block/Solver configurations..."
             << std::endl;

   std::multimap< std::string, netCDF::NcGroup > problems = f.getGroups();

   // For each problem descriptor
   for( auto & p : problems ) {

    // Deserialize block
    auto gb = p.second.getGroup( "Block" );
    auto block = Block::new_Block( gb );

    // Configure block
    auto bgc = p.second.getGroup( "BlockConfig" );
    auto b_config = static_cast<BlockConfig *>(BlockConfig::new_Configuration( bgc ));
    if( b_config ) {
     b_config->apply( block );
    }

    // Configure solver
    auto bgs = p.second.getGroup( "BlockSolver" );
    auto s_config = static_cast<BlockSolverConfig *>(BlockSolverConfig::new_Configuration( bgs ));
    if( s_config ) {
     s_config->apply( block );
    }

    std::cout << "Problem: " << p.first << std::endl;

    // Solve
    solve_all( block );
   }
   break;
  }

  case eBlockFile: {
   // Block file containing one or more Blocks

   std::cout << filename << " is a block file" << std::endl;

   std::multimap< std::string, netCDF::NcGroup > block_groups = f.getGroups();

   // For each Block
   for( auto bg : block_groups ) {

    // Deserialize block
    // auto class_name_attribute = bg.second.getAtt( "type" );
    // std::string class_name;
    // class_name_attribute.getValues( class_name );
    // std::cout << class_name << std::endl;

    auto block = Block::new_Block( bg.second );

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
      std::cerr << "block_solver: "
                << "Block configuration not valid: " << config_name
                << std::endl;
      exit( 1 );
     }

     try {
      bcf >> *b_config;
     } catch( const std::exception & e ) {
      std::cerr << "block_solver: "
                << "Block configuration not valid: " << e.what() << std::endl;
      exit( 1 );
     }

    } else {
     std::cout << "Block configuration not provided" << std::endl;
     // FIXME: Do not default on UCBlock
     b_config = default_configure_ucblock( block );
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
      std::cerr << "block_solver: "
                << "Solver configuration not valid: " << config_name
                << std::endl;
      exit( 1 );
     }

     try {
      scf >> *s_config;
     } catch( const std::exception & e ) {
      std::cerr << "block_solver: "
                << "Solver configuration not valid: " << e.what() << std::endl;
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
   }
   break;
  }

  default:
   std::cerr << "block_solver: "
             << filename << " is not a valid SMS++ file" << std::endl;
   exit( 1 );
 }

 return 0;
}
