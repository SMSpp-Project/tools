/*--------------------------------------------------------------------------*/
/*----------------------- File dual_trace.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * A small tool that solves the Lagrangian dual of a tiny problem with every
 * Solver of a BlockSolverConfig, in turn and from the same point, and writes
 * at every iteration of each of them a trace that the interactive pages of
 * opt-playground replay (https://github.com/dmeoli/opt-playground, see its
 * TRACE-FORMAT.md): the multipliers, the value, a subgradient, the solution
 * of the subproblem that gave it (the "column"), whether the step was
 * serious and the stability centre.
 *
 * The problem is
 *
 *   min { c x : a_i x <= b_i , i = 1, 2 , l <= x <= u } ,  x in R^2 ,
 *
 * whose two rows a_i x <= b_i are relaxed with multipliers y_i >= 0; the
 * inner Block, the box, is solved by BoxSolver, so that the columns are the
 * vertices of the box. The instance file holds, separated by blanks, c_1
 * c_2, l_1 u_1 l_2 u_2, then a_11 a_12 b_1 and a_21 a_22 b_2.
 *
 * Usage: dual_trace <instance> <BlockSolverConfig> <output prefix>
 *
 * Every Solver of the BlockSolverConfig, which configures the Block of the
 * multipliers, writes <output prefix>-<k>.json, k = 1, 2, ... The
 * ComputeConfig of each must set intEverykIt to 1, so that the handler is
 * called at every iteration. BundleSolver calls it after the master problem
 * and before the next evaluation, so the handler sees the evaluation of the
 * previous iteration; the last one is read after compute() returns.
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

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "AbstractBlock.h"
#include "BlockSolverConfig.h"
#include "BoxSolver.h"
#include "BundleSolver.h"
#include "FRealObjective.h"
#include "LagBFunction.h"
#include "LinearFunction.h"
#include "OneVarConstraint.h"
#include "SubGradSolver.h"

#include "common_utils.h"

#if __has_include( <SMS++_toolsConfig.h> )
 #include <SMS++_toolsConfig.h>
#endif

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

namespace {

// one oracle call of a run
struct Step {
 double y[ 2 ];       ///< the multipliers
 double value;        ///< L(y)
 double g[ 2 ];       ///< a subgradient, a_i x - b_i
 double x[ 2 ];       ///< the column: the solution of the subproblem
 bool serious;        ///< the step was serious
 double centre[ 2 ];  ///< the stability centre after the step
 };

// the instance: c, the box, the two relaxed rows
struct Instance {
 double c[ 2 ] , l[ 2 ] , u[ 2 ] , a[ 2 ][ 2 ] , b[ 2 ];

 explicit Instance( const std::string & file ) {
  std::ifstream in( file );
  if( ! ( in >> c[ 0 ] >> c[ 1 ] >> l[ 0 ] >> u[ 0 ] >> l[ 1 ] >> u[ 1 ]
	     >> a[ 0 ][ 0 ] >> a[ 0 ][ 1 ] >> b[ 0 ]
	     >> a[ 1 ][ 0 ] >> a[ 1 ][ 1 ] >> b[ 1 ] ) )
   throw( std::invalid_argument( "cannot read the instance " + file ) );
  }
 };

// the Lagrangian dual of the instance: the Block of the multipliers, whose
// Objective is the LagBFunction, and the inner Block, the box, solved by
// BoxSolver (as in the unit test of SubGradSolver)
struct LagDual {
 AbstractBlock * root;
 AbstractBlock * inner;
 std::vector< ColVariable > * y;
 std::vector< ColVariable > * x;
 std::vector< BoxConstraint > * box;
 LagBFunction * lbf;
 FRealObjective * robj;

 explicit LagDual( const Instance & I ) {
  inner = new AbstractBlock();
  x = new std::vector< ColVariable >( 2 );
  inner->add_static_variable( *x , "x" );
  box = new std::vector< BoxConstraint >( 2 );
  inner->add_static_constraint( *box , "box" );
  LinearFunction::v_coeff_pair cp;
  for( int j = 0 ; j < 2 ; ++j ) {
   ( *box )[ j ].set_variable( & ( *x )[ j ] , eNoMod );
   ( *box )[ j ].set_lhs( I.l[ j ] , eNoMod );
   ( *box )[ j ].set_rhs( I.u[ j ] , eNoMod );
   cp.push_back( { & ( *x )[ j ] , I.c[ j ] } );
   }
  auto iobj = new FRealObjective( inner ,
				  new LinearFunction( std::move( cp ) ) );
  iobj->set_sense( Objective::eMin , eNoMod );
  inner->set_objective( iobj , eNoMod );

  root = new AbstractBlock();
  y = new std::vector< ColVariable >( 2 );
  for( auto & yi : *y )
   yi.is_positive( true , eNoMod );
  root->add_static_variable( *y , "y" );

  lbf = new LagBFunction( inner );
  inner->register_Solver( new BoxSolver() );
  LagBFunction::v_dual_pair dp;
  for( int i = 0 ; i < 2 ; ++i ) {
   // the row a_i x - b_i <= 0, with y_i as multiplier
   LinearFunction::v_coeff_pair ai;
   for( int j = 0 ; j < 2 ; ++j )
    ai.push_back( { & ( *x )[ j ] , I.a[ i ][ j ] } );
   dp.push_back( LagBFunction::dual_pair(
		    & ( *y )[ i ] , new LinearFunction( std::move( ai ) ,
							 - I.b[ i ] ) ) );
   }
  lbf->set_dual_pairs( std::move( dp ) );

  robj = new FRealObjective( root , lbf );
  robj->set_sense( Objective::eMax , eNoMod );
  root->set_objective( robj , eNoMod );
  }

 ~LagDual() {
  inner->unregister_Solvers( true );
  robj->set_function( nullptr , eNoMod , true );
  delete root;
  delete robj;
  delete y;
  delete box;
  delete x;
  }

 // the last evaluation: the multipliers in y, the value and the
 // linearization of the LagBFunction; asking for the latter also writes the
 // solution of the inner Solver into x
 Step last( void ) {
  Step s;
  for( int i = 0 ; i < 2 ; ++i )
   s.y[ i ] = ( *y )[ i ].get_value();
  s.value = lbf->get_value();
  lbf->get_linearization_coefficients( s.g );
  for( int j = 0 ; j < 2 ; ++j )
   s.x[ j ] = ( *x )[ j ].get_value();
  s.serious = true;
  s.centre[ 0 ] = s.y[ 0 ];
  s.centre[ 1 ] = s.y[ 1 ];
  return( s );
  }
 };

std::string pair( const double * v ) {
 std::ostringstream o;
 o << std::setprecision( 12 ) << "[" << v[ 0 ] << ", " << v[ 1 ] << "]";
 return( o.str() );
 }

// the trace in the format of opt-playground (kind "dual2d"); the columns of
// a box are its vertices, and the polygon is the box itself
void write( const std::string & file , const std::string & source ,
	    const Instance & I , const std::vector< Step > & steps ) {
 std::ofstream o( file );
 const double V[ 4 ][ 2 ] = { { I.l[ 0 ] , I.l[ 1 ] } ,
			      { I.u[ 0 ] , I.l[ 1 ] } ,
			      { I.u[ 0 ] , I.u[ 1 ] } ,
			      { I.l[ 0 ] , I.u[ 1 ] } };
 double lmax = 1;
 for( const auto & s : steps )
  lmax = std::max( { lmax , 1.3 * s.y[ 0 ] , 1.3 * s.y[ 1 ] } );
 o << "{ \"kind\": \"dual2d\", \"source\": \"SMS++ " << source << "\",\n"
   << "  \"problem\": { \"points\": [" << pair( V[ 0 ] ) << ", "
   << pair( V[ 1 ] ) << ", " << pair( V[ 2 ] ) << ", " << pair( V[ 3 ] )
   << "],\n    \"polygon\": [" << pair( V[ 0 ] ) << ", " << pair( V[ 1 ] )
   << ", " << pair( V[ 2 ] ) << ", " << pair( V[ 3 ] ) << "],\n"
   << "    \"c\": " << pair( I.c ) << ", \"A\": [" << pair( I.a[ 0 ] )
   << ", " << pair( I.a[ 1 ] ) << "], \"b\": " << pair( I.b )
   << ", \"lmax\": " << std::ceil( lmax ) << " },\n  \"iterations\": [\n";
 for( std::size_t k = 0 ; k < steps.size() ; ++k ) {
  const auto & s = steps[ k ];
  o << "    { \"lam\": " << pair( s.y ) << ", \"value\": "
    << std::setprecision( 12 ) << s.value << ", \"subgradient\": "
    << pair( s.g ) << ", \"column\": " << pair( s.x ) << ", \"serious\": "
    << ( s.serious ? "true" : "false" ) << ", \"center\": "
    << pair( s.centre ) << " }" << ( k + 1 < steps.size() ? "," : "" )
    << "\n";
  }
 o << "  ] }\n";
 }

void usage( std::ostream & os , const char * exe ) {
 os << "Usage: " << exe << " <instance> <BlockSolverConfig> <output prefix>"
    << std::endl
    << "  or:  " << exe << " -h | --help" << std::endl
    << "  or:  " << exe << " -V | --version" << std::endl;
 }

void docopt( const char * exe ) {
 usage( std::cout , exe );
 std::cout << std::endl
  << "Solves the Lagrangian dual of min { c x : A x <= b , l <= x <= u },\n"
     "x in R^2, with the two rows of A relaxed, by every Solver of the\n"
     "<BlockSolverConfig> in turn, from y = 0, and writes for each the trace\n"
     "<output prefix>-<k>.json that opt-playground replays.\n"
     "\n"
     "The <instance> holds c_1 c_2, l_1 u_1 l_2 u_2, a_11 a_12 b_1 and\n"
     "a_21 a_22 b_2, separated by blanks.\n"
     "\n"
     "Options:\n"
     "  -h, --help     print this help\n"
     "  -V, --version  print the SMS++ tools version and exit\n"
     "\n"
     "Examples:\n"
     "  smspp_dual_trace box.txt DualBSCfg.txt box\n"
     "      writes box-1.json, box-2.json, ... (run in dual_trace/config)\n"
     "\n"
     "Exit status:\n"
     "  0 if all the traces are written, nonzero otherwise." << std::endl;
 }

}  // end of the unnamed namespace

/*--------------------------------------------------------------------------*/
/*-------------------------------- main() ----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 if( argc == 2 ) {
  const std::string opt( argv[ 1 ] );
  if( ( opt == "-h" ) || ( opt == "--help" ) ) {
   docopt( argv[ 0 ] );
   return( 0 );
   }
  if( ( opt == "-V" ) || ( opt == "--version" ) ) {
   #ifdef SMSpp_tools_VERSION
    std::cout << "SMS++ tools version " << SMSpp_tools_VERSION << std::endl;
   #else
    std::cout << "SMS++ tools version not available" << std::endl;
   #endif
   return( 0 );
   }
  }

 if( argc != 4 ) {
  usage( std::cerr , argv[ 0 ] );
  return( 1 );
  }

 try {
  Instance I( argv[ 1 ] );
  LagDual D( I );

  auto c = Configuration::deserialize( argv[ 2 ] );
  auto bsc = dynamic_cast< BlockSolverConfig * >( c );
  if( ! bsc ) {
   delete c;
   throw( std::invalid_argument( std::string( argv[ 2 ] ) +
				 " is not a BlockSolverConfig" ) );
   }
  bsc->apply( D.root );

  int k = 0;
  for( auto * slv : D.root->get_registered_solvers() ) {
   ++k;
   for( auto & yi : *D.y )   // every Solver starts from y = 0
    yi.set_value( 0 );

   std::vector< Step > steps;
   double centre[ 2 ] = { 0 , 0 };
   double fbar = - Inf< double >();
   auto bs = dynamic_cast< BundleSolver * >( slv );
   auto record = [ & ]( void ) {
    Step s;
    try {      // before the first evaluation there is nothing to read
     s = D.last();
     }
    catch( std::exception & ) {
     return;
     }
    if( bs ) {  // BundleSolver knows its centre: a step is serious when
                // the centre has moved onto the point just evaluated
     const auto & L = bs->get_current_point();
     if( L.size() == 2 ) {
      centre[ 0 ] = L[ 0 ];
      centre[ 1 ] = L[ 1 ];
      }
     s.serious = std::abs( centre[ 0 ] - s.y[ 0 ] ) < 1e-12 &&
		 std::abs( centre[ 1 ] - s.y[ 1 ] ) < 1e-12;
     }
    else {      // the others: serious when the value improves
     s.serious = s.value > fbar;
     if( s.serious ) {
      fbar = s.value;
      centre[ 0 ] = s.y[ 0 ];
      centre[ 1 ] = s.y[ 1 ];
      }
     }
    s.centre[ 0 ] = centre[ 0 ];
    s.centre[ 1 ] = centre[ 1 ];
    // the same evaluation seen twice is recorded once
    if( steps.empty() || steps.back().y[ 0 ] != s.y[ 0 ] ||
	steps.back().y[ 1 ] != s.y[ 1 ] )
     steps.push_back( s );
    };

   auto handler = [ & ]( void ) {
    record();
    return( ThinComputeInterface::eContinue );
    };
   auto id = slv->set_event_handler( ThinComputeInterface::eEverykIteration ,
				     handler );
   int st = slv->compute( false );
   record();   // the last evaluation, after the last event
   slv->reset_event_handler( ThinComputeInterface::eEverykIteration , id );

   std::string name = bs ? "BundleSolver" :
    dynamic_cast< SubGradSolver * >( slv ) ? "SubGradSolver" : "Solver";
   std::string out = std::string( argv[ 3 ] ) + "-" + std::to_string( k ) +
		     ".json";
   write( out , name , I , steps );
   std::cout << name << " #" << k << ": status " << st << ", "
	     << steps.size() << " evaluations, last value "
	     << std::setprecision( 10 ) << steps.back().value << " -> "
	     << out << std::endl;
   }

  bsc->clear();
  bsc->apply( D.root );
  delete bsc;
  }
 catch( std::exception & e ) {
  std::cerr << e.what() << std::endl;
  return( 1 );
  }

 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*---------------------- End File dual_trace.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
