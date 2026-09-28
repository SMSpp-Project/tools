/*--------------------------------------------------------------------------*/
/*------------------------- File reopt_bench.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * The driver of the computational study on the re-optimization of the
 * Min-Cost Flow problem. It loads a MCFBlock out of a DIMACS or a netCDF
 * file, applies a BlockSolverConfig to it, keeps of the latter the one
 * Solver chosen by -k, i.e., one method of the study, solves the instance,
 * and then changes it and solves it again for a number of rounds, printing
 * one line of comma-separated values per solve:
 *
 *   instance,method,kind,seed,round,status,value,time
 *
 * where round 0 is the first solve, kind is what the rounds change, value
 * the optimal value the Solver gives (empty if it gives none) and time that
 * of compute() alone, i.e., of taking in the changes and re-optimizing, the
 * reading of the instance, the construction of the Solver and the changes
 * made to the MCFBlock being excluded. One process runs one method, and the
 * changes depend only on the instance, the kind and the seed, so that the
 * methods run in different processes solve the same sequence of instances
 * and their values can be compared round by round.
 *
 * The changes of a round, each on max( 1 , f m ) arcs (or nodes) chosen at
 * random, m being the number of arcs, are computed from the original data,
 * so that the instance does not drift away from the one the generator gave:
 *
 * - cost: the cost of each arc becomes its original one times a factor
 *   uniform in [ 0.5 , 1.5 ], rounded (an arc of zero cost gets a cost
 *   uniform in [ 0 , the largest one ]);
 *
 * - cap: the same for the capacity of each arc, infinite ones left alone;
 *
 * - dfct: the deficits are given back their original value, then each of
 *   f m pairs made of a node with supply and one with demand has both its
 *   supply and its demand lowered by the same amount, uniform in [ 0 , half
 *   the smaller of the two ], so that they still sum to zero;
 *
 * - arcs: the arcs closed in the previous round are opened again and f m
 *   open arcs are closed, which is what a decomposition that fixes arcs at
 *   each iteration does;
 *
 * - mix: one of the four above, chosen at random at each round;
 *
 * - lag: the costs of all the arcs change a little at each round, as those
 *   of the subproblem of a Lagrangian relaxation do along a subgradient
 *   method: the cost of an arc is its original one plus a multiplier, which
 *   starts from zero and at round r moves by f C / sqrt( r ) times a
 *   standard normal step, kept nonnegative, C being the largest cost; the
 *   costs are fractional, and f is the step rather than a fraction of the
 *   arcs.
 *
 * With -r the rounds are not generated but read from a trace that
 * reopt_record wrote [see MCFTrace.h], i.e., they are the sequences of
 * instances that an application (say, a Lagrangian relaxation) has solved,
 * one per subproblem, which are replayed interleaved as they were solved;
 * then the seed in the output is the index of the subproblem, the round the
 * position in its sequence, and kind is given by -w (lds by default).
 * Since every method replays the same trace, their values can still be
 * compared round by round, and each one is also compared with the value
 * that MCFSimplex gave in the recording run, a difference being reported
 * on the standard error.
 *
 * Usage:
 *
 *   reopt_bench [-c PATH] [-S FILE] [-k N] [-m NAME] [-w KIND] [-n N]
 *               [-f F] [-e N] < dmx-or-nc4-file >
 *   reopt_bench -r [-c PATH] [-S FILE] [-k N] [-m NAME] [-w KIND]
 *               < trace-file >
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

#include <algorithm>
#include <cstdint>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>

#include <unistd.h>

#include <BlockSolverConfig.h>

#include "MCFBlock.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

using Index = MCFBlock::Index;
using Subset = MCFBlock::Subset;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static const char * const usage =
 "usage: reopt_bench [options] < dmx-or-nc4-file >\n"
 "  -c PATH  prefix of the configuration files [config/]\n"
 "  -S FILE  BlockSolverConfig of the MCFBlock [MCFBSCfg.txt]\n"
 "  -k N     index of the Solver of the BlockSolverConfig to run [0]\n"
 "  -m NAME  name of the method in the output [the Solver's index]\n"
 "  -w KIND  what the rounds change: cost, cap, dfct, arcs, mix or lag\n"
 "           [cost]\n"
 "  -n N     number of rounds after the first solve [100]\n"
 "  -f F     fraction of the arcs changed at each round [0.01]\n"
 "  -e N     seed of the changes [1]\n"
 "  -r       the file is a trace of reopt_record, whose rounds are replayed\n";

/*--------------------------------------------------------------------------*/
/// f m elements out of 0 .. n - 1, distinct and ordered

static Subset pick( std::mt19937 & rg , Index n , Index k )
{
 k = std::min( k , n );
 Subset all( n );
 for( Index i = 0 ; i < n ; ++i )
  all[ i ] = i;
 for( Index i = 0 ; i < k ; ++i )
  std::swap( all[ i ] ,
	     all[ std::uniform_int_distribution< Index >( i , n - 1 )( rg ) ] );
 Subset nms( all.begin() , all.begin() + k );
 std::sort( nms.begin() , nms.end() );
 return( nms );
 }

/*--------------------------------------------------------------------------*/
/// the k-th Solver of bsc applied to MCFB, the others dropped

static Solver * attach( MCFBlock * MCFB , BlockSolverConfig * bsc , int k )
{
 bsc->apply( MCFB );
 const auto & slvrs = MCFB->get_registered_solvers();
 Solver * solver = *std::next( slvrs.begin() , k );
 for( Index i = slvrs.size() ; i-- > 0 ; )
  if( Index( k ) != i ) {
   auto s = *std::next( slvrs.begin() , i );
   MCFB->unregister_Solver( s , true );
   }
 return( solver );
 }

/*--------------------------------------------------------------------------*/
/// replays the trace in fn with the k-th Solver of bsc

template< class T > static T get( std::istream & in ) {
 T v;
 in.read( reinterpret_cast< char * >( & v ) , sizeof( T ) );
 return( v );
 }

template< class T > static std::vector< T > get( std::istream & in ,
						 std::size_t n ) {
 std::vector< T > v( n );
 in.read( reinterpret_cast< char * >( v.data() ) , n * sizeof( T ) );
 return( v );
 }

static int replay( const std::string & fn , const std::string & instance ,
		   BlockSolverConfig * bsc , int k ,
		   const std::string & method , const std::string & kind )
{
 std::ifstream in( fn , std::ios::binary );
 if( ! in ) {
  std::cerr << "reopt_bench: cannot read " << fn << std::endl;
  return( 1 );
  }

 struct Sub {
  MCFBlock * MCFB = nullptr;
  Solver * solver = nullptr;
  std::vector< char > closed;
  int round = 0;
  double value = std::numeric_limits< double >::quiet_NaN();
  };
 std::map< std::uint32_t , Sub > subs;

 std::cout << std::setprecision( 12 );
 auto solve = [ & ]( std::uint32_t id , Sub & sb ) {
  const auto start = std::chrono::steady_clock::now();
  const int status = sb.solver->compute( false );
  const std::chrono::duration< double > t =
                                     std::chrono::steady_clock::now() - start;
  std::cout << instance << "," << method << "," << kind << "," << id
	    << "," << sb.round++ << "," << status << ",";
  sb.value = std::numeric_limits< double >::quiet_NaN();
  if( ( status == Solver::kOK ) || ( status == Solver::kLowPrecision ) ) {
   sb.value = sb.solver->get_var_value();
   std::cout << sb.value;
   }
  std::cout << "," << t.count() << std::endl;
  };

 // the (index, value) pairs of a 'D' record, as a Subset and a vector
 auto pairs = [ & ]( Subset & nms , std::vector< double > & val ) {
  const auto cnt = get< std::uint32_t >( in );
  nms.resize( cnt );
  val.resize( cnt );
  for( std::uint32_t i = 0 ; i < cnt ; ++i ) {
   nms[ i ] = get< std::uint32_t >( in );
   val[ i ] = get< double >( in );
   }
  };

 for( char tag ; in.get( tag ) ; ) {
  const auto id = get< std::uint32_t >( in );
  if( tag == 'F' ) {  // a new subproblem, all its data
   const auto n = get< std::uint32_t >( in );
   const auto m = get< std::uint32_t >( in );
   auto sn = get< std::uint32_t >( in , m );
   auto en = get< std::uint32_t >( in , m );
   auto c = get< double >( in , m );
   auto u = get< double >( in , m );
   auto b = get< double >( in , n );
   auto cl = get< char >( in , m );
   auto & sb = subs[ id ];
   if( sb.MCFB ) {
    std::cerr << "reopt_bench: subproblem " << id << " twice in " << fn
	      << std::endl;
    return( 1 );
    }
   sb.MCFB = new MCFBlock;
   sb.MCFB->load( n , m , Subset( en.begin() , en.end() ) ,
		  Subset( sn.begin() , sn.end() ) , u , c , b );
   sb.MCFB->generate_abstract_variables();
   sb.solver = attach( sb.MCFB , bsc , k );
   sb.closed.assign( m , 0 );
   Subset cls;
   for( std::uint32_t i = 0 ; i < m ; ++i )
    if( cl[ i ] )
     cls.push_back( i );
   if( ! cls.empty() ) {
    for( auto i : cls )
     sb.closed[ i ] = 1;
    sb.MCFB->close_arcs( std::move( cls ) , true );
    }
   solve( id , sb );
   }
  else
   if( tag == 'D' ) {  // the changes since the previous solve
    auto it = subs.find( id );
    if( it == subs.end() ) {
     std::cerr << "reopt_bench: changes of the unknown subproblem " << id
	       << " in " << fn << std::endl;
     return( 1 );
     }
    auto & sb = it->second;
    Subset nms;
    std::vector< double > val;
    pairs( nms , val );
    if( ! nms.empty() )
     sb.MCFB->chg_costs( val.cbegin() , std::move( nms ) , true );
    pairs( nms , val );
    if( ! nms.empty() )
     sb.MCFB->chg_ucaps( val.cbegin() , std::move( nms ) , true );
    pairs( nms , val );
    if( ! nms.empty() )
     sb.MCFB->chg_dfcts( val.cbegin() , std::move( nms ) , true );
    const auto tgl = get< std::uint32_t >( in , get< std::uint32_t >( in ) );
    Subset cls , opn;
    for( auto i : tgl )
     ( sb.closed[ i ] ? opn : cls ).push_back( i );
    for( auto i : tgl )
     sb.closed[ i ] = ! sb.closed[ i ];
    if( ! opn.empty() )
     sb.MCFB->open_arcs( std::move( opn ) , true );
    if( ! cls.empty() )
     sb.MCFB->close_arcs( std::move( cls ) , true );
    solve( id , sb );
    }
   else
    if( tag == 'V' ) {  // what MCFSimplex gave in the recording run
     const auto status = get< std::uint32_t >( in );
     const auto value = get< double >( in );
     auto it = subs.find( id );
     if( ( it != subs.end() ) && ( status == 1 ) &&
	 ( ! ( std::abs( it->second.value - value ) <=
	       1e-9 * std::max( 1.0 , std::abs( value ) ) ) ) )
      std::cerr << "reopt_bench: " << instance << " subproblem " << id
		<< " round " << it->second.round - 1 << ": " << method
		<< " gives " << it->second.value << ", the recording run "
		<< value << std::endl;
     }
    else {
     std::cerr << "reopt_bench: unknown record " << int( tag ) << " in "
	       << fn << std::endl;
     return( 1 );
     }
  if( ! in ) {
   std::cerr << "reopt_bench: " << fn << " truncated" << std::endl;
   return( 1 );
   }
  }

 for( auto & [ id , sb ] : subs ) {
  sb.MCFB->unregister_Solver( sb.solver , true );
  delete sb.MCFB;
  }
 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*-------------------------------- main() ----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 std::string prefix = "config/";
 std::string sconf_file = "MCFBSCfg.txt";
 std::string method;
 std::string kind;
 bool trace = false;
 int k = 0;
 int rounds = 100;
 double frac = 0.01;
 unsigned int seed = 1;

 for( int opt ; ( opt = getopt( argc , argv , "c:S:k:m:w:n:f:e:r" ) ) != -1 ; )
  switch( opt ) {
   case 'c': prefix = optarg; break;
   case 'S': sconf_file = optarg; break;
   case 'k': k = std::stoi( optarg ); break;
   case 'm': method = optarg; break;
   case 'w': kind = optarg; break;
   case 'n': rounds = std::stoi( optarg ); break;
   case 'f': frac = std::stod( optarg ); break;
   case 'e': seed = std::stoul( optarg ); break;
   case 'r': trace = true; break;
   default: std::cerr << usage; return( 1 );
   }

 if( optind != argc - 1 ) {
  std::cerr << usage;
  return( 1 );
  }

 if( kind.empty() )
  kind = trace ? "lds" : "cost";
 static const std::vector< std::string > kinds =
  { "cost" , "cap" , "dfct" , "arcs" , "mix" , "lag" };
 if( ( ! trace ) && std::find( kinds.begin() , kinds.end() , kind ) == kinds.end() ) {
  std::cerr << "reopt_bench: unknown kind " << kind << std::endl << usage;
  return( 1 );
  }

 // the files the configuration includes are read with the same prefix
 if( ( ! prefix.empty() ) && ( prefix.back() != '/' ) )
  prefix += '/';
 Configuration::set_filename_prefix( std::string( prefix ) );

 const std::string fn( argv[ optind ] );
 std::string instance = fn.substr( fn.find_last_of( '/' ) + 1 );
 instance = instance.substr( 0 , instance.find_last_of( '.' ) );
 if( method.empty() )
  method = std::to_string( k );

 // the Solver: the k-th of the BlockSolverConfig, the others dropped - - - -

 auto bsc = dynamic_cast< BlockSolverConfig * >(
				   Configuration::deserialize( sconf_file ) );
 if( ! bsc ) {
  std::cerr << "reopt_bench: " << prefix + sconf_file
	    << " is not a BlockSolverConfig" << std::endl;
  return( 1 );
  }
 if( ( k < 0 ) || ( Index( k ) >= bsc->num_ComputeConfig() ) ) {
  std::cerr << "reopt_bench: no Solver " << k << " in " << sconf_file
	    << std::endl;
  return( 1 );
  }

 if( trace ) {
  const int rc = replay( fn , instance , bsc , k , method , kind );
  delete bsc;
  return( rc );
  }

 // the MCFBlock- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 MCFBlock * MCFB = nullptr;
 if( ( fn.size() > 4 ) && ( fn.substr( fn.size() - 4 ) == ".nc4" ) )
  MCFB = dynamic_cast< MCFBlock * >( Block::deserialize( fn ) );
 else {
  std::ifstream in( fn );
  if( in ) {
   MCFB = new MCFBlock;
   MCFB->load( in );
   }
  }
 if( ! MCFB ) {
  std::cerr << "reopt_bench: no MCFBlock in " << fn << std::endl;
  return( 1 );
  }

 // an arc is closed by fixing its flow Variable, which have to be there
 MCFB->generate_abstract_variables();

 Solver * solver = attach( MCFB , bsc , k );

 // the original data, which the changes are computed from - - - - - - - - -

 const Index m = MCFB->get_NArcs();
 const Index n = MCFB->get_NNodes();
 const Index nchg = std::max( Index( 1 ) , Index( frac * m ) );
 const auto C0 = MCFB->get_C();
 const auto U0 = MCFB->get_U();
 const auto B0 = MCFB->get_B();
 const double cmax = C0.empty() ? 1 :
  std::abs( *std::max_element( C0.begin() , C0.end() ,
			       []( double a , double b ) {
				return( std::abs( a ) < std::abs( b ) ); } ) );
 Subset sources , sinks;
 for( Index i = 0 ; i < B0.size() ; ++i )
  if( B0[ i ] < 0 )
   sources.push_back( i );  // deficit < 0 means supply
  else
   if( B0[ i ] > 0 )
    sinks.push_back( i );

 std::mt19937 rg( seed );
 std::uniform_real_distribution< double > U01( 0 , 1 );
 std::normal_distribution< double > N01( 0 , 1 );
 std::vector< double > lambda( m , 0 );  // the multipliers of "lag"
 Subset closed;

 // one solve, one line- - - - - - - - - - - - - - - - - - - - - - - - - - -

 std::cout << std::setprecision( 12 );
 auto solve = [ & ]( int r , const std::string & what ) {
  const auto start = std::chrono::steady_clock::now();
  const int status = solver->compute( false );
  const std::chrono::duration< double > t =
                                     std::chrono::steady_clock::now() - start;
  std::cout << instance << "," << method << "," << what << "," << seed
	    << "," << r << "," << status << ",";
  if( ( status == Solver::kOK ) || ( status == Solver::kLowPrecision ) )
   std::cout << solver->get_var_value();
  std::cout << "," << t.count() << std::endl;
  };

 solve( 0 , kind );

 for( int r = 1 ; r <= rounds ; ++r ) {
  std::string what = kind;
  if( kind == "mix" )
   what = kinds[ std::uniform_int_distribution< int >( 0 , 3 )( rg ) ];

  if( what == "lag" ) {
   const double step = frac * cmax / std::sqrt( double( r ) );
   MCFBlock::Vec_CNumber nc( m );
   for( Index j = 0 ; j < m ; ++j ) {
    lambda[ j ] = std::max( 0.0 , lambda[ j ] + step * N01( rg ) );
    nc[ j ] = ( C0.empty() ? 0 : C0[ j ] ) + lambda[ j ];
    }
   MCFB->chg_costs( nc.cbegin() , MCFBlock::Range( 0 , m ) , true );
   }
  else
  if( what == "cost" ) {
   auto nms = pick( rg , m , nchg );
   MCFBlock::Vec_CNumber nc( nms.size() );
   for( Index j = 0 ; j < nms.size() ; ++j ) {
    const double c = C0.empty() ? 0 : C0[ nms[ j ] ];
    nc[ j ] = c != 0 ? std::round( c * ( 0.5 + U01( rg ) ) )
                     : std::round( U01( rg ) * cmax );
    }
   MCFB->chg_costs( nc.cbegin() , std::move( nms ) , true );
   }
  else
   if( what == "cap" ) {
    auto nms = pick( rg , m , nchg );
    MCFBlock::Vec_FNumber nu( nms.size() );
    for( Index j = 0 ; j < nms.size() ; ++j ) {
     const double u = U0.empty() ? Inf< double >() : U0[ nms[ j ] ];
     nu[ j ] = u < Inf< double >() ? std::round( u * ( 0.5 + U01( rg ) ) )
                                   : u;
     }
    MCFB->chg_ucaps( nu.cbegin() , std::move( nms ) , true );
    }
   else
    if( what == "dfct" ) {
     auto B = B0;
     if( ( ! sources.empty() ) && ( ! sinks.empty() ) )
      for( Index j = 0 ; j < nchg ; ++j ) {
       const Index s = sources[ std::uniform_int_distribution< Index >(
					       0 , sources.size() - 1 )( rg ) ];
       const Index t = sinks[ std::uniform_int_distribution< Index >(
						 0 , sinks.size() - 1 )( rg ) ];
       const double d = std::floor( U01( rg ) * 0.5 *
				    std::min( - B[ s ] , B[ t ] ) );
       B[ s ] += d;
       B[ t ] -= d;
       }
     MCFB->chg_dfcts( B.cbegin() , MCFBlock::Range( 0 , n ) );
     }
    else {  // arcs
     if( ! closed.empty() )
      MCFB->open_arcs( Subset( closed ) , true );
     closed = pick( rg , m , nchg );
     MCFB->close_arcs( Subset( closed ) , true );
     }

  solve( r , what );
  }

 // clean up- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 MCFB->unregister_Solver( solver , true );
 delete bsc;
 delete MCFB;

 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*---------------------- End File reopt_bench.cpp --------------------------*/
/*--------------------------------------------------------------------------*/
