/*--------------------------------------------------------------------------*/
/*---------------------------- ml_utils.cpp --------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of the model-agnostic machine learning scaffolding declared
 * in ml_utils.h.
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

#include "ml_utils.h"

#include <algorithm>

#include <cmath>

#include <map>

#include <numeric>

#include <random>

#include <sstream>

#include <stdexcept>

/*--------------------------------------------------------------------------*/
/*------------------------- SPLITTING THE DATA -----------------------------*/
/*--------------------------------------------------------------------------*/

IndexSet shuffled_indices( std::size_t n , unsigned seed )
{
 IndexSet idx( n );
 std::iota( idx.begin() , idx.end() , std::size_t( 0 ) );

 std::mt19937 rng( seed );
 std::shuffle( idx.begin() , idx.end() , rng );

 return( idx );

 }  // end( shuffled_indices )

/*--------------------------------------------------------------------------*/
/// the samples, shuffled, grouped by the class they belong to
/** Returns the indices of the \p n samples shuffled out of \p seed and split
 * into one group per distinct value of \p labels, the groups being in a
 * deterministic order since std::map is sorted by key. With \p labels empty
 * there is a single group holding all the samples, which is what turns every
 * stratified procedure below into its plain counterpart.
 *
 * Splitting *each group* is the whole point: dealing a single interleaved
 * order out to k parts would alias, since the period with which a class
 * recurs in such an order and the number of parts need not be coprime, and a
 * part would then get a wildly wrong share of that class. */

static std::vector< IndexSet > class_groups(
                             std::size_t n , unsigned seed ,
                             const std::vector< double > & labels )
{
 auto order = shuffled_indices( n , seed );

 if( labels.empty() ) {
  std::vector< IndexSet > one;
  one.push_back( std::move( order ) );
  return( one );
  }

 if( labels.size() != n )
  throw( std::invalid_argument( "ml_utils: labels have the wrong size" ) );

 std::map< double , IndexSet > by_class;
 for( auto i : order )
  by_class[ labels[ i ] ].push_back( i );

 std::vector< IndexSet > groups;
 groups.reserve( by_class.size() );
 for( auto & g : by_class )
  groups.push_back( std::move( g.second ) );

 return( groups );

 }  // end( class_groups )

/*--------------------------------------------------------------------------*/

DataSplit train_test_split( std::size_t n , double test_fraction ,
                            unsigned seed ,
                            const std::vector< double > & labels )
{
 if( ( test_fraction <= 0 ) || ( test_fraction >= 1 ) )
  throw( std::invalid_argument( "ml_utils: the test fraction must be "
                                "strictly between 0 and 1" ) );

 const auto n_test = std::max( std::size_t( 1 ) ,
                               std::size_t( n * test_fraction ) );

 auto groups = class_groups( n , seed , labels );

 /* Each class contributes to the held-out part in proportion to its size.
  * Rounding each share down would leave some samples to be assigned, so they
  * go to the classes with the largest fractional remainder: this is the
  * largest-remainder rule, and it makes the shares add up to exactly n_test
  * while keeping every one of them within one sample of its proportion. */
 std::vector< std::size_t > share( groups.size() );
 std::vector< std::pair< double , std::size_t > > remainder( groups.size() );
 std::size_t assigned = 0;

 for( std::size_t g = 0 ; g < groups.size() ; ++g ) {
  const double exact = double( groups[ g ].size() ) * n_test / n;
  share[ g ] = std::size_t( exact );
  remainder[ g ] = { exact - share[ g ] , g };
  assigned += share[ g ];
  }

 std::sort( remainder.begin() , remainder.end() ,
            []( auto & a , auto & b ) { return( a.first > b.first ); } );

 for( std::size_t r = 0 ; assigned < n_test ; ++r , ++assigned ) {
  auto g = remainder[ r % remainder.size() ].second;
  if( share[ g ] < groups[ g ].size() )
   ++share[ g ];
  else
   --assigned;  // that class is exhausted: try the next one
  }

 DataSplit split;
 split.test.reserve( n_test );
 split.train.reserve( n - n_test );

 for( std::size_t g = 0 ; g < groups.size() ; ++g ) {
  auto & group = groups[ g ];
  split.test.insert( split.test.end() ,
                     group.begin() , group.begin() + share[ g ] );
  split.train.insert( split.train.end() ,
                      group.begin() + share[ g ] , group.end() );
  }

 if( split.train.empty() )
  throw( std::invalid_argument( "ml_utils: the training part is empty" ) );

 return( split );

 }  // end( train_test_split )

/*--------------------------------------------------------------------------*/

std::vector< DataSplit > k_fold( std::size_t n , unsigned k , unsigned seed ,
                                 const std::vector< double > & labels )
{
 if( k < 2 )
  throw( std::invalid_argument( "ml_utils: the folds must be at least two" ) );

 if( k > n )
  throw( std::invalid_argument( "ml_utils: more folds than samples" ) );

 auto groups = class_groups( n , seed , labels );

 /* Every class is dealt out to the k folds round-robin on its own, so each
  * fold gets its size divided by k, rounded either way, of every class. The
  * starting fold rotates from one class to the next so that the remainders
  * do not all pile up on the first folds. */
 std::vector< IndexSet > folds( k );
 std::size_t start = 0;

 for( auto & group : groups ) {
  for( std::size_t t = 0 ; t < group.size() ; ++t )
   folds[ ( start + t ) % k ].push_back( group[ t ] );
  start = ( start + group.size() ) % k;
  }

 std::vector< DataSplit > splits( k );
 for( unsigned f = 0 ; f < k ; ++f ) {
  splits[ f ].test = folds[ f ];
  for( unsigned g = 0 ; g < k ; ++g )
   if( g != f )
    splits[ f ].train.insert( splits[ f ].train.end() ,
                              folds[ g ].begin() , folds[ g ].end() );
  }

 return( splits );

 }  // end( k_fold )

/*--------------------------------------------------------------------------*/
/*------------------------------- SCORES -----------------------------------*/
/*--------------------------------------------------------------------------*/

/// throws exception unless the two vectors are nonempty and of equal size

static void check_sizes( const std::vector< double > & y_true ,
                         const std::vector< double > & y_pred )
{
 if( y_true.size() != y_pred.size() )
  throw( std::invalid_argument( "ml_utils: the targets and the predictions "
                                "have different sizes" ) );
 if( y_true.empty() )
  throw( std::invalid_argument( "ml_utils: no sample to score" ) );
 }

/*--------------------------------------------------------------------------*/

double accuracy( const std::vector< double > & y_true ,
                 const std::vector< double > & y_pred )
{
 check_sizes( y_true , y_pred );

 std::size_t right = 0;
 for( std::size_t i = 0 ; i < y_true.size() ; ++i )
  if( y_true[ i ] == y_pred[ i ] )
   ++right;

 return( double( right ) / y_true.size() );

 }  // end( accuracy )

/*--------------------------------------------------------------------------*/

double mean_squared_error( const std::vector< double > & y_true ,
                           const std::vector< double > & y_pred )
{
 check_sizes( y_true , y_pred );

 double s = 0;
 for( std::size_t i = 0 ; i < y_true.size() ; ++i ) {
  const double e = y_true[ i ] - y_pred[ i ];
  s += e * e;
  }

 return( s / y_true.size() );

 }  // end( mean_squared_error )

/*--------------------------------------------------------------------------*/

double mean_absolute_error( const std::vector< double > & y_true ,
                            const std::vector< double > & y_pred )
{
 check_sizes( y_true , y_pred );

 double s = 0;
 for( std::size_t i = 0 ; i < y_true.size() ; ++i )
  s += std::abs( y_true[ i ] - y_pred[ i ] );

 return( s / y_true.size() );

 }  // end( mean_absolute_error )

/*--------------------------------------------------------------------------*/

double neg_mean_squared_error( const std::vector< double > & y_true ,
                               const std::vector< double > & y_pred )
{
 return( - mean_squared_error( y_true , y_pred ) );
 }

/*--------------------------------------------------------------------------*/

double neg_mean_absolute_error( const std::vector< double > & y_true ,
                                const std::vector< double > & y_pred )
{
 return( - mean_absolute_error( y_true , y_pred ) );
 }

/*--------------------------------------------------------------------------*/

double r2_score( const std::vector< double > & y_true ,
                 const std::vector< double > & y_pred )
{
 check_sizes( y_true , y_pred );

 double mean = 0;
 for( auto y : y_true )
  mean += y;
 mean /= y_true.size();

 double ss_res = 0 , ss_tot = 0;
 for( std::size_t i = 0 ; i < y_true.size() ; ++i ) {
  const double e = y_true[ i ] - y_pred[ i ];
  const double d = y_true[ i ] - mean;
  ss_res += e * e;
  ss_tot += d * d;
  }

 return( ss_tot > 0 ? 1 - ss_res / ss_tot : 0 );

 }  // end( r2_score )

/*--------------------------------------------------------------------------*/
/*-------------------------------- GRID ------------------------------------*/
/*--------------------------------------------------------------------------*/

Grid parse_grid( const std::string & spec )
{
 Grid grid;

 std::istringstream axes( spec );
 std::string axis;

 while( std::getline( axes , axis , ';' ) ) {
  if( axis.empty() )
   continue;

  const auto eq = axis.find( '=' );
  if( ( eq == std::string::npos ) || ( ! eq ) ||
      ( eq + 1 == axis.size() ) )
   throw( std::invalid_argument( "ml_utils: malformed grid axis \"" + axis +
                                 "\", expected name=v1,v2,..." ) );

  GridAxis ga( axis.substr( 0 , eq ) , std::vector< double >() );

  std::istringstream values( axis.substr( eq + 1 ) );
  std::string value;

  while( std::getline( values , value , ',' ) ) {
   if( value.empty() )
    continue;
   try {
    ga.second.push_back( std::stod( value ) );
    }
   catch( ... ) {
    throw( std::invalid_argument( "ml_utils: \"" + value +
                                  "\" is not a number in the grid axis \"" +
                                  ga.first + "\"" ) );
    }
   }

  if( ga.second.empty() )
   throw( std::invalid_argument( "ml_utils: no value in the grid axis \"" +
                                 ga.first + "\"" ) );

  grid.push_back( std::move( ga ) );
  }

 return( grid );

 }  // end( parse_grid )

/*--------------------------------------------------------------------------*/

std::vector< GridPoint > grid_points( const Grid & grid )
{
 // the Cartesian product built one axis at a time, so that the last axis is
 // the one varying fastest; with no axis at all the only point is the empty
 // one, i.e., "change nothing", which is what makes the caller uniform
 std::vector< GridPoint > points( 1 );

 for( auto & axis : grid ) {
  std::vector< GridPoint > next;
  next.reserve( points.size() * axis.second.size() );

  for( auto & point : points )
   for( auto value : axis.second ) {
    next.push_back( point );
    next.back().push_back( value );
    }

  points = std::move( next );
  }

 return( points );

 }  // end( grid_points )

/*--------------------------------------------------------------------------*/

std::string to_string( const Grid & grid , const GridPoint & point )
{
 if( grid.size() != point.size() )
  throw( std::invalid_argument( "ml_utils: the point does not belong to the "
                                "grid" ) );

 std::ostringstream out;
 for( std::size_t a = 0 ; a < grid.size() ; ++a ) {
  if( a )
   out << ", ";
  out << grid[ a ].first << " = " << point[ a ];
  }

 return( out.str() );

 }  // end( to_string )

/*--------------------------------------------------------------------------*/
/*------------------------- End ml_utils.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
