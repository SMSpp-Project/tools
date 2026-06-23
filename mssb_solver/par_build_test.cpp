/*--------------------------------------------------------------------------*/
/*------------------------ File par_build_test.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Stand-alone micro-benchmark for the *construction* (de-serialization) of a
 * MultiStageStochasticBlock, used to study the parallelization of building the
 * inner Block tree. It only de-serializes the instance (it does NOT configure
 * or solve it), so the timing window is short and isolates the construction.
 *
 * The MSSB de-serializer itself reports the time spent building its L inner
 * Blocks (the parallelizable loop) when the env var MSSB_PAR_BUILD=N is set:
 * N=1 builds them serially, N>1 on N threads. So:
 *
 *   MSSB_PAR_BUILD=1 ./par_build_test examples/big_baked_L8.nc4   # serial
 *   MSSB_PAR_BUILD=8 ./par_build_test examples/big_baked_L8.nc4   # 8 threads
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <chrono>
#include <iostream>

#include "MultiStageStochasticBlock.h"

#include "UCBlock.h"

/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 if( argc < 2 ) {
  std::cerr << "usage: " << argv[ 0 ] << " <instance.nc4>" << std::endl;
  return( 1 );
  }

 netCDF::NcFile f( argv[ 1 ] , netCDF::NcFile::read );
 auto group = f.getGroup( "Block_0" );
 if( group.isNull() ) {
  std::cerr << "no 'Block_0' group in " << argv[ 1 ] << std::endl;
  return( 1 );
  }

 const auto t0 = std::chrono::steady_clock::now();
 Block * block = Block::new_Block( group , nullptr );
 const auto t1 = std::chrono::steady_clock::now();

 if( ! dynamic_cast< MultiStageStochasticBlock * >( block ) ) {
  std::cerr << "Error: 'Block_0' is not a MultiStageStochasticBlock"
            << std::endl;
  delete block;
  return( 1 );
  }

 std::cout << "total Block construction = "
           << std::chrono::duration< double >( t1 - t0 ).count() << "s"
           << std::endl;

 delete block;
 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*-------------------- End File par_build_test.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
