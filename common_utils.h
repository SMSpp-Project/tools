/** @file
 * Some common utilities for SMS++ tools.
 *
 */

#ifndef __COMMON_UTILS
#define __COMMON_UTILS

#include <getopt.h>

#include <Block.h>
#include <RBlockConfig.h>

#include <UnitBlock.h>
#include <HydroSystemUnitBlock.h>

using namespace SMSpp_di_unipi_it;

std::string filename{};
std::string bconf_file{};
std::string sconf_file{};
bool solvVerbose = false;
bool writeprob = false;
std::string exe{};
std::string docopt_desc{};

/*--------------------------------------------------------------------------*/

std::string get_filename( const std::string & fullpath ) {
 std::size_t found = fullpath.find_last_of( "/\\" );
 return fullpath.substr( found + 1 );
}

/*--------------------------------------------------------------------------*/

void docopt() {
 // http://docopt.org
 std::cout << docopt_desc << std::endl;
 std::cout << "Usage: " << exe << " [options] <nc4-file>\n"
           << std::endl
           << "Options:\n"
           << "  -B <file>, --blockcfg <file>    Block configuration.\n"
           << "  -S <file>, --solvercfg <file>   Solver configuration.\n"
           << "  -n <file>, --nc4problem <file>  Write nc4 problem on file.\n"
           << "  -v, --verbose                   Make the solver verbose.\n"
           << "  -h, --help                      Print this help.\n";
}

/*--------------------------------------------------------------------------*/

void process_args( int argc, char ** argv ) {

 if( argc < 2 ) {
  std::cout << exe << ": no input file\n"
            << "Try " << exe << "' --help' for more information.\n";
  exit( 1 );
 }

 const char * const short_opts = "B:S:nvh";
 const option long_opts[] = {
  { "blockcfg",   required_argument, nullptr, 'B' },
  { "solvercfg",  required_argument, nullptr, 'S' },
  { "nc4problem", no_argument,       nullptr, 'n' },
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
   case 'n':
    writeprob = true;
    break;
   case 'v':
    solvVerbose = true;
    break;
   case 'h':
    docopt();
    exit( 0 );
   case '?':
   default:
    std::cout << "Try " << exe << "' --help' for more information.\n";
    exit( 1 );
  }
 }

 // Last argument
 if( optind < argc ) {
  filename = std::string( argv[ optind ] );
 } else {
  std::cout << exe << ": no input file\n"
            << "Try " << exe << "' --help' for more information.\n";
  exit( 1 );
 }
}

/*--------------------------------------------------------------------------*/

// Returns a default ThermalUnitBlock configuration
BlockConfig * default_configure_thermalunitblock() {
 auto conf = new BlockConfig();
 conf->f_static_variables_Configuration = new SimpleConfiguration< int >( 15 );
 return conf;
}

/*--------------------------------------------------------------------------*/

// Returns a default UCBlock configuration
BlockConfig * default_configure_ucblock( Block * uc_block ) {

 BlockConfig * b_config = new RBlockConfig;

 for( auto sb: uc_block->get_nested_Blocks() ) {
  if( !dynamic_cast<UnitBlock *>( sb ) ) {
   continue;
  }

  // Common UnitBlock static variables configuration
  auto sbc = new RBlockConfig;
  sbc->f_static_variables_Configuration =
   new SimpleConfiguration< int >( 15 );

  // If HydroSystemUnitBlock, we configure its PolyhedralFunctionBlocks
  auto hu_block = dynamic_cast<HydroSystemUnitBlock *>( sb );

  if( hu_block != nullptr ) {
   auto num_nested_blocks_hydro = hu_block->get_number_nested_Blocks();
   for( auto ssb: hu_block->get_nested_Blocks() ) {

    auto pf_block = dynamic_cast<PolyhedralFunctionBlock *>( ssb );

    if( pf_block != nullptr ) {
     auto ssbc = new BlockConfig();
     ssbc->f_static_variables_Configuration =
      new SimpleConfiguration< int >( 1 );

     int idx = ssb->get_nested_Block_index( sb );
     sbc->add_sub_BlockConfig( ssbc, idx );
    }
   }
  }

  int idx = sb->get_nested_Block_index( uc_block );
  static_cast<RBlockConfig *>( b_config )->add_sub_BlockConfig( sbc, idx );
 }

 return b_config;
}

// BlockConfig * default_configure_ucblock( Block * uc_block ) {
//
//  BlockConfig * b_config = new RBlockConfig;
//  auto num_nested_blocks = uc_block->get_number_nested_Blocks();
//  for( Block::Index i = 0; i < num_nested_blocks; ++i ) {
//
//   auto sub_block = uc_block->get_nested_Block( i );
//   if( !dynamic_cast<UnitBlock *>( sub_block ) )
//    continue;
//
//   auto subconf = new RBlockConfig;
//   subconf->f_static_variables_Configuration =
//    new SimpleConfiguration< int >( 15 );
//
//   auto hu_block = dynamic_cast<HydroSystemUnitBlock *>( sub_block );
//
//   if( hu_block != nullptr ) {
//    auto num_nested_blocks_hydro = hu_block->get_number_nested_Blocks();
//    for( Block::Index j = 0; j < num_nested_blocks_hydro; ++j ) {
//     auto sub_Block_hydro = hu_block->get_nested_Block( j );
//     auto sub_pf_block =
//      dynamic_cast<PolyhedralFunctionBlock *>( sub_Block_hydro );
//
//     if( sub_pf_block != nullptr ) {
//      auto subsubconf = new BlockConfig();
//      subsubconf->f_static_variables_Configuration =
//       new SimpleConfiguration< int >( 1 );
//      subconf->add_sub_BlockConfig( subsubconf, j );
//     }
//    }
//   }
//
//   static_cast<RBlockConfig *>( b_config )->add_sub_BlockConfig( subconf, i );
//  }
//
//  return b_config;
// }

/*--------------------------------------------------------------------------*/

// Returns a default Solver configuration
BlockSolverConfig * default_configure_solver( int verbose ) {
 auto s_config = new BlockSolverConfig;
 auto c_config = new ComputeConfig;

 if( verbose ) {
  c_config->set_par( "intLogVerb", 1 );
 }

 s_config->add_ComputeConfig( "CPXMILPSolver", c_config );
 return s_config;
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

// Configures a Block with a BlockConfig file
BlockConfig * configure_block( Block * block, const std::string & conf_file ) {
 BlockConfig * b_config = nullptr;
 std::ifstream bcf;

 bcf.open( conf_file, std::ifstream::in );
 if( !bcf.is_open() ) {
  return nullptr;
 }

 std::string name;
 bcf >> eatcomments >> name;
 b_config = dynamic_cast<BlockConfig *> ( Configuration::new_Configuration( name ) );

 if( !b_config ) {
  return nullptr;
 }

 try {
  bcf >> *b_config;
 } catch( const std::exception & e ) {
  return nullptr;
 }

 b_config->apply( block );
 return b_config;
}

/*--------------------------------------------------------------------------*/

// Configures a Block with a BlockSolverConfig file
BlockSolverConfig *
configure_blocksolver( Block * block, const std::string & conf_file ) {
 BlockSolverConfig * s_config = nullptr;
 std::ifstream scf;

 scf.open( conf_file, std::ifstream::in );
 if( !scf.is_open() ) {
  return nullptr;
 }

 std::string name;
 scf >> eatcomments >> name;
 s_config = dynamic_cast<BlockSolverConfig *> ( Configuration::new_Configuration( name ) );

 if( !s_config ) {
  return nullptr;
 }

 try {
  scf >> *s_config;
 } catch( const std::exception & e ) {
  return nullptr;
 }

 s_config->apply( block );
 return s_config;
}

/*--------------------------------------------------------------------------*/

// Writes a new nc4 problem using the block and its configurations
void write_nc4problem( Block * block,
                       BlockConfig * b_config,
                       BlockSolverConfig * s_config ) {

 std::size_t found = filename.find_last_of( '.' );
 std::string nc4_file = filename.substr( 0, found ) + "_problem.nc4";

 netCDF::NcFile outfile;
 try {
  outfile.open( nc4_file, netCDF::NcFile::replace );
 } catch( netCDF::exceptions::NcException & e ) {
  std::cerr << exe << ": cannot open nc4 file " << nc4_file << std::endl;
  exit( 1 );
 }
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

/*--------------------------------------------------------------------------*/


#endif //__COMMON_UTILS
