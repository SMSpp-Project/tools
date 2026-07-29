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
#include <cstdlib>
#include <exception>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

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

 auto * mssb = dynamic_cast< MultiStageStochasticBlock * >( block );
 if( ! mssb ) {
  std::cerr << "Error: 'Block_0' is not a MultiStageStochasticBlock"
            << std::endl;
  delete block;
  return( 1 );
  }

 std::cout << "total Block construction = "
           << std::chrono::duration< double >( t1 - t0 ).count() << "s"
           << std::endl;

 // Model-build phase: generate_abstract_variables/constraints on each inner
 // sub-Block. Unlike construction (de-serialization), this is CPU-bound and
 // does NOT touch netCDF, so it is the candidate for a real parallel speedup.
 // MSSB_PAR_GEN=N>1 runs the per-sub-Block model build on N threads.
 using Index = Block::Index;
 const Index L = mssb->get_number_sub_blocks();
 const char * gen_env = std::getenv( "MSSB_PAR_GEN" );
 const int g_thr = gen_env ? std::atoi( gen_env ) : 0;

 auto gen_one = [ & ]( Index l ) {
  Block * s = mssb->get_sub_Block( l );
  s->generate_abstract_variables();
  s->generate_abstract_constraints();
  };

 const auto g0 = std::chrono::steady_clock::now();
 if( g_thr <= 1 )
  for( Index l = 0 ; l < L ; ++l )
   gen_one( l );
 else {
  std::exception_ptr err;
  std::mutex mx;
  std::vector< std::thread > pool;
  for( int t = 0 ; t < g_thr ; ++t )
   pool.emplace_back( [ & , t ]() {
    try {
     for( Index l = static_cast< Index >( t ) ; l < L ;
          l += static_cast< Index >( g_thr ) )
      gen_one( l );
     }
    catch( ... ) {
     std::lock_guard< std::mutex > lk( mx );
     if( ! err ) err = std::current_exception();
     }
    } );
  for( auto & th : pool )
   th.join();
  if( err )
   std::rethrow_exception( err );
  }
 const auto g1 = std::chrono::steady_clock::now();
 std::cout << "[gen_abstract] L=" << L << " threads="
           << ( g_thr > 1 ? g_thr : 1 ) << " time="
           << std::chrono::duration< double >( g1 - g0 ).count() << "s"
           << std::endl;

 delete block;
 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*-------------------- End File par_build_test.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
