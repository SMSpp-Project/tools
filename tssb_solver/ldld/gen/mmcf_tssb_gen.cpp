/*--------------------------------------------------------------------------*/
/*-------------------------- File mmcf_tssb_gen.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * Writes a stochastic multicommodity fixed-charge network design instance
 * as a TwoStageStochasticBlock: the scenario sub-Block is the MMCFBlock of a
 * deterministic instance (e.g., one of the Canad ones), the uncertainty is on
 * the demands of the commodities and the here-and-now decisions are the arcs
 * to build. The instance is meant for the knapsack structure of the
 * MMCFBlock, in which arc i is a BinaryKnapsackBlock whose items are the
 * flows of the commodities and, last, the design variable of the arc, and so
 * the here-and-now Variable are given by one AbstractPath per arc, to that
 * item.
 *
 *   mmcf_tssb_gen -i <instance> -o <output.nc4> [-f <format>] [-n <scenarios>]
 *                 [-v <variation>] [-u <upward variation>] [-s <seed>]
 *                 [-r <penalty>]
 *
 * The demand of commodity k in a scenario is its deterministic demand times
 * a multiplier drawn uniformly in [ 1 - v , 1 + u ], u being v unless given,
 * independently for every commodity and scenario; the scenarios have the
 * same probability. With u = 0 the demands only decrease, which keeps every
 * scenario feasible when the deterministic instance is and its capacities
 * are tight, as in small.std.
 *
 * With -r (Canad format only), the instance gets complete recourse: for each
 * commodity, two arcs between its origin and its destination, one per
 * direction, with no fixed cost, a capacity that no scenario can exceed and
 * a unit cost that is <penalty> times the largest unit cost of the
 * instance, i.e., the demand that the network built cannot carry is served
 * at a high price. The recourse arcs are not design decisions (they have no
 * AbstractPath), i.e., they are always there, at their price. Every scenario
 * is then feasible whatever arcs are built,
 * which Benders' decomposition needs when its subproblems are solved by a
 * Lagrangian dual, which gives no Farkas certificate. The
 * format of the deterministic instance is that of
 * MMCFBlock::load( std::istream & , char ), 's' (the Canad one) by default.
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
#include <sstream>
#include <string>
#include <vector>

#include <getopt.h>

#include <MMCFBlock.h>
#include <DiscreteScenarioSet.h>

/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/

static void usage( const char * exe )
{
 std::cerr << "usage: " << exe << " -i <instance> -o <output.nc4>"
           << " [-f <format>] [-n <scenarios>] [-v <variation>]"
           << " [-u <upward variation>] [-s <seed>] [-r <penalty>]"
           << std::endl;
 exit( 1 );
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 std::string input , output;
 char format = 0;
 int N = 10;
 double v = 0.2;
 double u = -1;  // the upward variation, v unless given
 double r = 0;   // the penalty of the complete recourse, none if 0
 unsigned seed = 1;

 int opt;
 while( ( opt = getopt( argc , argv , "i:o:f:n:v:u:s:r:h" ) ) != -1 )
  switch( opt ) {
   case 'i': input = optarg; break;
   case 'o': output = optarg; break;
   case 'f': format = optarg[ 0 ]; break;
   case 'n': N = std::atoi( optarg ); break;
   case 'v': v = std::atof( optarg ); break;
   case 'u': u = std::atof( optarg ); break;
   case 'r': r = std::atof( optarg ); break;
   case 's': seed = std::atoi( optarg ); break;
   default: usage( argv[ 0 ] );
   }
 if( input.empty() || output.empty() || ( N < 1 ) || ( v < 0 ) || ( v >= 1 ) )
  usage( argv[ 0 ] );

 // the deterministic instance- - - - - - - - - - - - - - - - - - - - - - - -

 std::ifstream in( input );
 if( ! in ) {
  std::cerr << "mmcf_tssb_gen: cannot open " << input << std::endl;
  return( 1 );
  }
 if( u < 0 )
  u = v;

 MMCFBlock mmcf;
 std::size_t design = 0;  // how many arcs are design decisions, all if 0
 if( r > 0 ) {  // the complete recourse, written into the Canad text
  std::size_t nn , na , nk;
  in >> nn >> na >> nk;
  design = na;  // the recourse arcs come after the original ones
  std::ostringstream arcs;
  double maxc = 0;
  for( std::size_t i = 0 ; i < na ; ++i ) {
   std::size_t e , st , h;
   double f , cap;
   in >> e >> st >> f >> cap >> h;
   arcs << e << " " << st << " " << f << " " << cap << " " << h << "\n";
   for( ; h-- ; ) {
    std::size_t k;
    double c , uk;
    in >> k >> c >> uk;
    maxc = std::max( maxc , c );
    arcs << k << " " << c << " " << uk << "\n";
    }
   }
  std::vector< std::size_t > org( nk + 1 , 0 ) , dst( nk + 1 , 0 );
  std::vector< double > dem( nk + 1 , 0 );
  std::ostringstream nodes;
  for( std::size_t k , i ; in >> k >> i ; ) {
   double f;
   in >> f;
   nodes << k << " " << i << " " << f << "\n";
   if( f > 0 ) { org[ k ] = i; dem[ k ] = f; }
   if( f < 0 ) dst[ k ] = i;  // a node the commodity does not touch has 0
   }
  // every commodity is allowed on every recourse arc, as on the other arcs
  // of a Canad instance, at the price of the recourse
  double tot = 0;
  for( std::size_t k = 1 ; k <= nk ; ++k )
   tot += dem[ k ];
  const double cap = 2 * ( 1 + u ) * tot + 1;
  for( std::size_t k = 1 ; k <= nk ; ++k )
   for( auto [ a , b ] : { std::pair( org[ k ] , dst[ k ] ) ,
                           std::pair( dst[ k ] , org[ k ] ) } ) {
    arcs << a << " " << b << " 0 " << cap << " " << nk << "\n";
    for( std::size_t q = 1 ; q <= nk ; ++q )
     arcs << q << " " << r * maxc << " " << cap << "\n";
    }
  std::istringstream text( std::to_string( nn ) + " " +
                           std::to_string( na + 2 * nk ) + " " +
                           std::to_string( nk ) + "\n" + arcs.str() +
                           nodes.str() );
  mmcf.load( text , 's' );
  }
 else
  mmcf.load( in , format ? format : 's' );

 const auto nk = mmcf.get_NComm();
 // the here-and-now decisions: the arcs of the instance, not those of the
 // recourse, which are always there, at their price
 const auto na = design ? decltype( mmcf.get_NArcs() )( design )
                        : mmcf.get_NArcs();

 // the scenarios - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 std::mt19937 gen( seed );
 std::uniform_real_distribution<> mult( 1 - v , 1 + u );
 std::vector< std::vector< double > > scenarios( N ,
                                              std::vector< double >( nk ) );
 for( auto & s : scenarios )
  for( decltype( nk + 0 ) k = 0 ; k < nk ; ++k )
   s[ k ] = mmcf.get_demand( k ) * mult( gen );

 // the TwoStageStochasticBlock - - - - - - - - - - - - - - - - - - - - - - -

 netCDF::NcFile f( output , netCDF::NcFile::replace );
 f.putAtt( "SMS++_file_type" , netCDF::NcInt() , 1 );

 auto g = f.addGroup( "Block_0" );
 g.putAtt( "type" , "TwoStageStochasticBlock" );
 g.addDim( "NumberScenarios" , N );

 // one path per arc, from the MMCFBlock to the last item of its knapsack:
 // a 'B' node selecting the sub-Block i, then a 'V' node selecting the
 // element nk (the design variable) of its static group 0
 {
  auto pg = g.addGroup( "StaticAbstractPath" );
  auto pdim = pg.addDim( "PathDim" , na );
  auto tldim = pg.addDim( "PathTotalLength" , 2 * na );
  std::vector< unsigned int > start( na ) , group( 2 * na ) ,
                              element( 2 * na ) , range( 2 * na );
  std::string types;
  for( decltype( na + 0 ) i = 0 ; i < na ; ++i ) {
   start[ i ] = 2 * i;
   types += "BV";
   group[ 2 * i ] = i;
   element[ 2 * i ] = 0;
   range[ 2 * i ] = 0;
   group[ 2 * i + 1 ] = 0;
   element[ 2 * i + 1 ] = nk;
   range[ 2 * i + 1 ] = nk + 1;
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

 // the StochasticBlock: the MMCFBlock and the mapping of a scenario onto
 // the demands of the commodities
 {
  auto sg = g.addGroup( "StochasticBlock" );
  sg.putAtt( "type" , "StochasticBlock" );

  auto bg = sg.addGroup( "Block" );
  mmcf.serialize( bg );

  auto ndm = sg.addDim( "NumberDataMappings" , 1 );
  char dt = 'D';
  sg.addVar( "DataType" , netCDF::NcChar() , ndm ).putVar( &dt );
  char cl = 'B';
  sg.addVar( "Caller" , netCDF::NcChar() , ndm ).putVar( &cl );

  std::string fn = "MMCFBlock::chg_demands";
  sg.addVar( "FunctionName" , netCDF::NcString() , ndm ).putVar( { 0 } ,
                                                                   &fn );

  auto ssd = sg.addDim( "SetSizeDim" , 2 );
  std::vector< unsigned int > ss = { 0 , 0 };
  sg.addVar( "SetSize" , netCDF::NcUint() , ssd ).putVar( ss.data() );

  unsigned char ord = 0;
  sg.addVar( "Ordered" , netCDF::NcUbyte() , ndm ).putVar( &ord );

  auto sed = sg.addDim( "SetElementsDim" , 4 );
  std::vector< unsigned int > se = { 0 , ( unsigned int ) nk ,
                                     0 , ( unsigned int ) nk };
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

 std::cout << output << ": " << mmcf.get_NNodes() << " nodes, " << mmcf.get_NArcs()
           << " arcs, " << nk << " commodities, " << N << " scenarios"
           << std::endl;
 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*------------------------ End File mmcf_tssb_gen.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
