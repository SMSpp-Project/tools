/*--------------------------------------------------------------------------*/
/*--------------------------- block_solver.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * SMS++ generic block and problem solver.
 *
 * A tool that loads an SMS++ nc4 Block or Problem file and solves it.
 *
 * In the case of a Block file, i.e., a file that contains one or more Blocks,
 * it optionally configures all the Blocks with a BlockConfig and/or a
 * BlockSolverConfig, then it solves it with all the loaded solvers.
 *
 * In the case of a Problem file, i.e., one that contains one or more Problems
 * (with a problem being a Block/BlockConfig/BlockSolverConfig tuple),
 * it solves each problem with all the loaded solvers.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Niccolo' Iardella \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni, Niccolo' Iardella
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <iostream>
#include <iomanip>

/*!!
#ifdef USE_DL
 #include <dlfcn.h>
#endif
!!*/

#include <Block.h>
#include <BlockSolverConfig.h>

#include "common_utils.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*----------------------------------------------------------------------------

#ifdef USE_DL
 #if __APPLE__
  #define LIBEXT ".dylib"
 #else
  #define LIBEXT ".so"
 #endif

std::vector< void * > dl_handles;

const static std::map< std::string, std::string > class_to_lib{
 { "ThermalUnitBlock", "UCBlock" },
 { "UCBlock",          "UCBlock" },
 { "CPXMILPSolver",    "MILPSolver" },
 };

------------------------------------------------------------------------------

void load_library( const std::string & class_name )
{
 const std::string & lib = class_to_lib.at( class_name );
 auto lib_path = "lib" + lib + LIBEXT;
 void * handle = dlopen( lib_path.c_str(), RTLD_LAZY );

 if( ! handle ) {
  std::cerr << "Error:" << dlerror();
  exit( 1 );
  }
 else
  dl_handles.push_back( handle );
 }

void unload_libraries() {
 for( auto handle: dl_handles )
  dlclose( handle );
 }

#endif

----------------------------------------------------------------------------*/

int main( int argc, char ** argv )
{
 // manage options and help, see common_utils.h
 docopt_desc = "SMS++ generic block and problem solver";
 exe = get_filename( argv[ 0 ] );
 process_args( argc , argv );

 // read nc4 file
 netCDF::NcFile f;
 try {
  f.open( filename, netCDF::NcFile::read );
  }
 catch( netCDF::exceptions::NcException & e ) {
  std::cerr << exe << ": cannot open nc4 file " << filename << std::endl;
  exit( 1 );
  }

 netCDF::NcGroupAtt gtype = f.getAtt( "SMS++_file_type" );
 if( gtype.isNull() ) {
  std::cerr << exe << ": " << filename
	    << " is not an SMS++ nc4 file" << std::endl;
  exit( 1 );
  }

 int type;
 gtype.getValues( &type );

 if( ( type != eProbFile ) && ( type != eBlockFile ) ) {
  std::cerr << exe << ": " << filename << " is not a valid SMS++ file"
	    << std::endl;
  exit( 1 );
  }

 if( type == eProbFile )
  // problem file containing one or more Block/BlockConfig/BlockSolver sets
  std::cout << filename
	    << " is a problem file, ignoring Block/Solver configurations"
	    << std::endl;

 auto groups = f.getGroups();

 // for each sub-group
 for( auto & g : groups ) {
  Block * block = nullptr;
  BlockConfig * b_config = nullptr;
  BlockSolverConfig * s_config = nullptr;

  if( type == eProbFile ) {
   std::cout << "Problem: " << g.first << std::endl;
   get_all( g.second , block , b_config , s_config );
   }
  else {
   std::cout << "Block: " << g.first << std::endl;
   get_all( g.second , bconf_file , sconf_file ,
	    block , b_config , s_config );
   }

  solve_all( block );
  if( s_config )
   s_config->apply( block );

  delete s_config;
  delete b_config;
  delete block;
  }

 return( 0 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------- end block_solver.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/

