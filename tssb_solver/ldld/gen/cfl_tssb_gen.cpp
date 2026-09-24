/*--------------------------------------------------------------------------*/
/*-------------------------- File cfl_tssb_gen.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * Writes a stochastic capacitated facility location instance as a
 * TwoStageStochasticBlock: the scenario sub-Block is the
 * CapacitatedFacilityLocationBlock of a deterministic instance, the
 * uncertainty is on the demands of the customers and the here-and-now
 * decisions are the openings of the facilities. The instance is meant for
 * the knapsack formulation of the CapacitatedFacilityLocationBlock, in
 * which the opening of facility i is the last item of the i-th
 * BinaryKnapsackBlock, and so the here-and-now Variable are given by one
 * AbstractPath per facility, to that item.
 *
 *   cfl_tssb_gen -i <instance> -o <output.nc4> [-f <format>] [-n <scenarios>]
 *                [-v <variation>] [-s <seed>]
 *
 * The demand of customer j in a scenario is its deterministic demand times a
 * multiplier drawn uniformly in [ 1 - v , 1 + v ], independently for every
 * customer and scenario, rounded to an integer (at least 1), since the
 * dynamic programming solver of the knapsacks wants integer weights; a
 * scenario whose total demand exceeds 95% of the total capacity is scaled
 * down to it (rounding down), so that every scenario is feasible with all
 * the facilities open. The scenarios have the same probability.
 * The format of the deterministic instance is that of
 * CapacitatedFacilityLocationBlock::load( std::istream & , char ).
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni, Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include <getopt.h>

#include <CapacitatedFacilityLocationBlock.h>
#include <DiscreteScenarioSet.h>

/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/

static void usage( const char * exe )
{
 std::cerr << "usage: " << exe << " -i <instance> -o <output.nc4>"
           << " [-f <format>] [-n <scenarios>] [-v <variation>]"
           << " [-s <seed>]" << std::endl;
 exit( 1 );
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 std::string input , output;
 char format = 0;
 int N = 10;
 double v = 0.2;
 unsigned seed = 1;

 int opt;
 while( ( opt = getopt( argc , argv , "i:o:f:n:v:s:h" ) ) != -1 )
  switch( opt ) {
   case 'i': input = optarg; break;
   case 'o': output = optarg; break;
   case 'f': format = optarg[ 0 ]; break;
   case 'n': N = std::atoi( optarg ); break;
   case 'v': v = std::atof( optarg ); break;
   case 's': seed = std::atoi( optarg ); break;
   default: usage( argv[ 0 ] );
   }
 if( input.empty() || output.empty() || ( N < 1 ) || ( v < 0 ) || ( v >= 1 ) )
  usage( argv[ 0 ] );

 // the deterministic instance- - - - - - - - - - - - - - - - - - - - - - - -

 std::ifstream in( input );
 if( ! in ) {
  std::cerr << "cfl_tssb_gen: cannot open " << input << std::endl;
  return( 1 );
  }
 CapacitatedFacilityLocationBlock cfl;
 cfl.load( in , format );

 const auto nf = cfl.get_NFacilities();
 const auto nc = cfl.get_NCustomers();
 const auto & D = cfl.get_Demands();
 const auto & Q = cfl.get_Capacities();
 const double capacity = std::accumulate( Q.begin() , Q.end() , 0.0 );

 // the scenarios - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 std::mt19937 gen( seed );
 std::uniform_real_distribution<> mult( 1 - v , 1 + v );
 std::vector< std::vector< double > > scenarios( N ,
                                              std::vector< double >( nc ) );
 for( auto & s : scenarios ) {
  double total = 0;
  for( decltype( nc + 0 ) j = 0 ; j < nc ; ++j )
   total += ( s[ j ] = std::max( 1.0 , std::round( D[ j ] * mult( gen ) ) ) );
  if( total > 0.95 * capacity )
   for( auto & d : s )
    d = std::max( 1.0 , std::floor( d * 0.95 * capacity / total ) );
  }

 // the TwoStageStochasticBlock - - - - - - - - - - - - - - - - - - - - - - -

 netCDF::NcFile f( output , netCDF::NcFile::replace );
 f.putAtt( "SMS++_file_type" , netCDF::NcInt() , 1 );

 auto g = f.addGroup( "Block_0" );
 g.putAtt( "type" , "TwoStageStochasticBlock" );
 g.addDim( "NumberScenarios" , N );

 // one path per facility, from the CFL to the last item of its knapsack:
 // a 'B' node selecting the sub-Block i, then a 'V' node selecting the
 // element nc of its static group 0
 {
  auto pg = g.addGroup( "StaticAbstractPath" );
  auto pdim = pg.addDim( "PathDim" , nf );
  auto tldim = pg.addDim( "PathTotalLength" , 2 * nf );
  std::vector< unsigned int > start( nf ) , group( 2 * nf ) ,
                              element( 2 * nf ) , range( 2 * nf );
  std::string types;
  for( decltype( nf + 0 ) i = 0 ; i < nf ; ++i ) {
   start[ i ] = 2 * i;
   types += "BV";
   group[ 2 * i ] = i;
   element[ 2 * i ] = 0;
   range[ 2 * i ] = 0;
   group[ 2 * i + 1 ] = 0;
   element[ 2 * i + 1 ] = nc;
   range[ 2 * i + 1 ] = nc + 1;
   }
  pg.addVar( "PathStart" , netCDF::NcUint() , pdim ).putVar( start.data() );
  pg.addVar( "PathNodeTypes" , netCDF::NcChar() , tldim ).putVar(
                                                             types.data() );
  pg.addVar( "PathGroupIndices" , netCDF::NcUint() , tldim ).putVar(
                                                             group.data() );
  pg.addVar( "PathElementIndices" , netCDF::NcUint() , tldim ).putVar(
                                                           element.data() );
  pg.addVar( "PathRangeIndices" , netCDF::NcUint() , tldim ).putVar(
                                                             range.data() );
  }

 // the StochasticBlock: the CFL and the mapping of a scenario onto the
 // demands of the customers
 {
  auto sg = g.addGroup( "StochasticBlock" );
  sg.putAtt( "type" , "StochasticBlock" );

  auto bg = sg.addGroup( "Block" );
  cfl.serialize( bg );

  auto ndm = sg.addDim( "NumberDataMappings" , 1 );
  char dt = 'D';
  sg.addVar( "DataType" , netCDF::NcChar() , ndm ).putVar( &dt );
  char cl = 'B';
  sg.addVar( "Caller" , netCDF::NcChar() , ndm ).putVar( &cl );

  std::string fn = "CapacitatedFacilityLocationBlock::chg_customer_demands";
  sg.addVar( "FunctionName" , netCDF::NcString() , ndm ).putVar( { 0 } ,
                                                                   &fn );

  auto ssd = sg.addDim( "SetSizeDim" , 2 );
  std::vector< unsigned int > ss = { 0 , 0 };
  sg.addVar( "SetSize" , netCDF::NcUint() , ssd ).putVar( ss.data() );

  unsigned char ord = 0;
  sg.addVar( "Ordered" , netCDF::NcUbyte() , ndm ).putVar( &ord );

  auto sed = sg.addDim( "SetElementsDim" , 4 );
  std::vector< unsigned int > se = { 0 , ( unsigned int ) nc ,
                                     0 , ( unsigned int ) nc };
  sg.addVar( "SetElements" , netCDF::NcUint() , sed ).putVar( se.data() );

  auto apg = sg.addGroup( "AbstractPath" );  // empty: the Block itself
  auto apdim = apg.addDim( "PathDim" , 1 );
  auto aptldim = apg.addDim( "PathTotalLength" , 0 );
  unsigned int ps = 0;
  apg.addVar( "PathStart" , netCDF::NcUint() , apdim ).putVar( &ps );
  apg.addVar( "PathNodeTypes" , netCDF::NcChar() , aptldim );
  apg.addVar( "PathGroupIndices" , netCDF::NcUint() , aptldim );
  apg.addVar( "PathElementIndices" , netCDF::NcUint() , aptldim );
  apg.addVar( "PathRangeIndices" , netCDF::NcUint() , aptldim );
  }

 // the scenarios, by the serializer of the class
 {
  DiscreteScenarioSet dss;
  std::vector< double > weights( N , 1.0 / N );
  dss.load_from_memory( scenarios , weights );
  auto dg = g.addGroup( "DiscreteScenarioSet" );
  dss.serialize( dg );
  }

 std::cout << output << ": " << nf << " facilities, " << nc
           << " customers, " << N << " scenarios" << std::endl;
 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*------------------------ End File cfl_tssb_gen.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
