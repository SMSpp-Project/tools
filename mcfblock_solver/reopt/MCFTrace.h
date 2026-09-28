/*--------------------------------------------------------------------------*/
/*---------------------------- File MCFTrace.h -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * A MCFSimplex that records, at each call to SolveMCF(), the instance it is
 * about to solve, so that the sequence of Min-Cost Flow instances that an
 * application generates (say, the subproblems of a Lagrangian relaxation)
 * can be solved again, exactly the same, by every method of the study
 * [see reopt_bench -r]. This is the tester class of Frangioni and Manca
 * (2006) split in two: the recording run is guided by MCFSimplex, and the
 * others only replay what it solved, which is why the choice of the solver
 * cannot change the sequence.
 *
 * All the MCFTrace objects write to the same binary stream, set by
 * MCFTrace::open(), and each record starts with a one-byte tag and the
 * index of the object, given in the order of the first solve:
 *
 * - 'F' (the first solve of the object): n, m, the starting and the ending
 *   node of each arc (from 1 to n), the costs, the capacities, the deficits
 *   and, for each arc, one byte telling whether it is closed;
 *
 * - 'D' (each further solve): the costs, the capacities and the deficits
 *   that have changed since the previous solve of the object, each as a
 *   count followed by (index, value) pairs, then the count and the indices
 *   of the arcs that have been closed or opened;
 *
 * - 'V' (after each solve): the status of MCFSimplex and the optimal value.
 *
 * Indices and counts are 32-bit unsigned integers, values doubles, all in
 * the byte order of the machine that records. Writing is serialized by a
 * mutex, but the order of the records is the order of the solves only if
 * the subproblems are solved one at a time, which the recording run has to
 * ensure (say, by not letting LagrangianDualSolver solve them in parallel).
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*----------------------------- DEFINITIONS --------------------------------*/
/*--------------------------------------------------------------------------*/

#ifndef __MCFTrace
 #define __MCFTrace
                      /* self-identification: #endif at the end of the file */

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <cstdint>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include "MCFSimplex.h"

/*--------------------------------------------------------------------------*/
/*--------------------------- CLASS MCFTrace -------------------------------*/
/*--------------------------------------------------------------------------*/

namespace MCFClass_di_unipi_it
{
 class MCFTrace : public MCFSimplex
 {
 public:

  MCFTrace( Index nmx = 0 , Index mmx = 0 ) : MCFSimplex( nmx , mmx ) {}

  /// the file all the MCFTrace write to, truncated
  static void open( const std::string & name ) {
   out().open( name , std::ios::binary | std::ios::trunc );
   if( ! out() )
    throw( std::runtime_error( "MCFTrace::open: cannot write " + name ) );
   }

  static void close( void ) { out().close(); }

  /// the number of instances solved so far by all the MCFTrace
  static unsigned long solves( void ) { return( nsolves() ); }

  void SolveMCF( void ) override {
   std::lock_guard< std::mutex > guard( mtx() );
   ++nsolves();
   if( out().is_open() ) {
    record();
    out().flush();  // the instance is on file even if the solve never ends
    }
   MCFSimplex::SolveMCF();
   if( out().is_open() ) {
    put( 'V' );
    put( std::uint32_t( id ) );
    put( std::uint32_t( MCFGetStatus() + 1 ) );
    put( double( MCFGetFO() ) );
    }
   }

 private:

  static std::ofstream & out( void ) { static std::ofstream o; return( o ); }
  static std::mutex & mtx( void ) { static std::mutex x; return( x ); }
  static Index & count( void ) { static Index c = 0; return( c ); }
  static unsigned long & nsolves( void ) {
   static unsigned long s = 0; return( s ); }

  template< class T > static void put( const T & v ) {
   out().write( reinterpret_cast< const char * >( & v ) , sizeof( T ) );
   }

  template< class T > static void put( const std::vector< T > & v ) {
   out().write( reinterpret_cast< const char * >( v.data() ) ,
		v.size() * sizeof( T ) );
   }

  // the changed entries of old w.r.t. now, which becomes the old one
  static void put_diff( std::vector< double > & old ,
			const std::vector< double > & now ) {
   std::vector< std::uint32_t > idx;
   for( std::uint32_t i = 0 ; i < now.size() ; ++i )
    if( now[ i ] != old[ i ] )
     idx.push_back( i );
   put( std::uint32_t( idx.size() ) );
   for( auto i : idx ) {
    put( i );
    put( now[ i ] );
    old[ i ] = now[ i ];
    }
   }

  void record( void ) {
   const Index nn = MCFn();
   const Index mm = MCFm();
   std::vector< double > c( mm ) , u( mm ) , b( nn );
   MCFCosts( c.data() );
   MCFUCaps( u.data() );
   MCFDfcts( b.data() );
   std::vector< char > cl( mm );
   for( Index i = 0 ; i < mm ; ++i )
    cl[ i ] = IsClosedArc( i ) ? 1 : 0;

   if( id == Inf< Index >() ) {  // the first solve: all the data
    id = count()++;
    std::vector< Index > sn( mm ) , en( mm );
    MCFArcs( sn.data() , en.data() );
    put( 'F' );
    put( std::uint32_t( id ) );
    put( std::uint32_t( nn ) );
    put( std::uint32_t( mm ) );
    for( auto v : sn ) put( std::uint32_t( v ) );
    for( auto v : en ) put( std::uint32_t( v ) );
    put( c );
    put( u );
    put( b );
    put( cl );
    C = std::move( c );
    U = std::move( u );
    B = std::move( b );
    CL = std::move( cl );
    return;
    }

   if( ( nn != B.size() ) || ( mm != C.size() ) )
    throw( std::logic_error( "MCFTrace::record: the graph has changed" ) );

   put( 'D' );
   put( std::uint32_t( id ) );
   put_diff( C , c );
   put_diff( U , u );
   put_diff( B , b );
   std::vector< std::uint32_t > tgl;
   for( std::uint32_t i = 0 ; i < mm ; ++i )
    if( cl[ i ] != CL[ i ] )
     tgl.push_back( i );
   put( std::uint32_t( tgl.size() ) );
   put( tgl );
   CL = std::move( cl );
   }

  Index id = Inf< Index >();     ///< index in the order of the first solve
  std::vector< double > C , U , B;  ///< the data at the previous solve
  std::vector< char > CL;           ///< the closed arcs at the previous solve

 };  // end( class( MCFTrace ) )

}  // end( namespace MCFClass_di_unipi_it )

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

#endif  /* MCFTrace.h included */

/*--------------------------------------------------------------------------*/
/*------------------------- End File MCFTrace.h ----------------------------*/
/*--------------------------------------------------------------------------*/
