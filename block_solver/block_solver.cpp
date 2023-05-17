/** @file
 * SMS++ generic block and problem solver.
 *
 * A tool that loads a SMS++ nc4 Block or Problem file and solves it.
 *
 * In case of a Block file, i.e., a file that contains one or more Blocks,
 * it optionally configures all the Blocks with a BlockConfig and/or a
 * BlockSolverConfig, then it solves it with all the loaded solvers.
 *
 * In case of a Problem file, i.e., one that contains one or more Problems
 * (with a problem being a Block/BlockConfig/BlockSolverConfig tuple),
 * it solves each problem with all the loaded solvers.
 *
 * \author Niccolo' Iardella \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Niccolo' Iardella
 */

#include <iostream>
#include <iomanip>

#ifdef USE_DL

#include <dlfcn.h>

#endif

#include <Block.h>
#include <BlockSolverConfig.h>

#include "common_utils.h"

using namespace SMSpp_di_unipi_it;

#if __APPLE__
#define LIBEXT ".dylib"
#else
#define LIBEXT ".so"
#endif

/*--------------------------------------------------------------------------*/

#ifdef USE_DL
std::vector< void * > dl_handles;

const static std::map< std::string, std::string > class_to_lib{
 { "ThermalUnitBlock", "UCBlock" },
 { "UCBlock",          "UCBlock" },
 { "CPXMILPSolver",    "MILPSolver" },
};

/*--------------------------------------------------------------------------*/

void load_library( const std::string & class_name ) {

 const std::string & lib = class_to_lib.at( class_name );
 auto lib_path = "lib" + lib + LIBEXT;
 void * handle = dlopen( lib_path.c_str(), RTLD_LAZY );

 if( ! handle ) {
  std::cerr << "Error:" << dlerror();
  exit( 1 );
 } else {
  dl_handles.push_back( handle );
 }
}

void unload_libraries() {
 for( auto handle: dl_handles ) {
  dlclose( handle );
 }
}

#endif

/*--------------------------------------------------------------------------*/

int main( int argc, char ** argv ) {

 // Manage options and help, see common_utils.h
 docopt_desc = "SMS++ generic block and problem solver.\n";
 exe = get_filename( argv[ 0 ] );
 process_args( argc, argv );

 // Read nc4 file
 netCDF::NcFile f;
 try {
  f.open( filename, netCDF::NcFile::read );
 } catch( netCDF::exceptions::NcException & e ) {
  std::cerr << exe << ": cannot open nc4 file " << filename << std::endl;
  exit( 1 );
 }

 netCDF::NcGroupAtt gtype = f.getAtt( "SMS++_file_type" );
 if( gtype.isNull() ) {
  std::cerr << exe << ": "
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
#ifdef USE_DL
    netCDF::NcGroupAtt gbtype = gb.getAtt( "type" );
    std::string classname;
    gbtype.getValues( classname );
    load_library( classname );
#endif
    auto block = Block::new_Block( gb );

    // Configure block
    auto bgc = p.second.getGroup( "BlockConfig" );
    auto b_config = static_cast< BlockConfig * >( BlockConfig::new_Configuration( bgc ) );
    if( b_config ) {
     b_config->apply( block );
    }

    // Configure solver
    auto bgs = p.second.getGroup( "BlockSolver" );
    auto s_config = static_cast< BlockSolverConfig * >( BlockSolverConfig::new_Configuration( bgs ) );
    if( s_config ) {
#ifdef USE_DL
     for( const auto & solvername : s_config->get_SolverNames() ) {
      load_library( solvername );
     }
#endif
     s_config->apply( block );
    }

    std::cout << "Problem: " << p.first << std::endl;

    // Solve
    std::cout.setf( std::ios::scientific, std::ios::floatfield );
    std::cout << std::setprecision( 8 );
    solve_all( block );
   }
   break;
  }

  case eBlockFile: {
   // Block file containing one or more Blocks

   std::cout << filename << " is a block file" << std::endl;

   std::multimap< std::string, netCDF::NcGroup > block_groups = f.getGroups();

   // For each Block
   for( const auto & bg : block_groups ) {

    // Deserialize block
    auto gb = bg.second;
#ifdef USE_DL
    netCDF::NcGroupAtt gbtype = gb.getAtt( "type" );
    std::string classname;
    gbtype.getValues( classname );
    load_library( classname );
#endif
    auto block = Block::new_Block( gb );

    // Configure block
    BlockConfig * b_config;
    if( ! bconf_file.empty() ) {
     b_config = get_blockconfig( bconf_file );
     if( b_config == nullptr ) {
      std::cerr << exe << ": Block configuration not valid" << std::endl;
      exit( 1 );
     }
     b_config->apply( block );
    }

    // Configure solver
    BlockSolverConfig * s_config;
    if( ! sconf_file.empty() ) {
     s_config = get_blocksolverconfig( sconf_file );
     if( s_config == nullptr ) {
      std::cerr << exe << ": Block configuration not valid" << std::endl;
      exit( 1 );
     }
    } else {
     std::cout << "Using a default Solver configuration" << std::endl;
     s_config = default_configure_solver( solvVerbose );
    }

#ifdef USE_DL
    for( const auto & solvername : s_config->get_SolverNames() ) {
     load_library( solvername );
    }
#endif
    s_config->apply( block );

    // Solve
    std::cout.setf( std::ios::scientific, std::ios::floatfield );
    std::cout << std::setprecision( 8 );
    solve_all( block );
   }
   break;
  }

  default:
   std::cerr << exe << ": "
             << filename << " is not a valid SMS++ file" << std::endl;
   exit( 1 );
 }

#ifdef USE_DL
 unload_libraries();
#endif
 return( 0 );
}
