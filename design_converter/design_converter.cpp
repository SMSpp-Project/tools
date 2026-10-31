/*--------------------------------------------------------------------------*/
/*------------------------ File design_converter.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * A tool converting a netCDF file whose Block is in design mode into one of
 * InvestmentBlock.
 *
 * A Block in design mode decides the investment itself, with the design
 * Variable of its units and lines, and pays it in its own Objective: a
 * UCBlock whose units have an investment cost, or a DesignNetworkBlock, or a
 * StochasticBlock, a TwoStageStochasticBlock or a MultiStageStochasticBlock
 * holding such UCBlock, written to be solved as a whole. An InvestmentBlock
 * refuses such an inner Block [see InvestmentFunction::
 * resolve_asset_methods()], the investment being its own ColVariable, and
 * this tool writes the same problem as an InvestmentBlock instead: each
 * design becomes an asset with the cost and the bounds of the design, sized
 * as the class of the asset is, the units lose their investment cost, a
 * DesignNetworkBlock gives way to the NetworkBlock of its time instants, and
 * the first-stage paths to the designs are dropped.
 *
 * Usage:
 *
 *     design_converter <input file> <output file>
 *
 * The input is a Block file or a problem file [see eBlockFile and
 * eProbFile]. Each Block in it is written in the output file
 *
 * - converted, if it is an InvestmentBlock whose inner Block is in design
 *   mode, the designs becoming the assets of the InvestmentBlock, which has
 *   to declare none;
 *
 * - inside a new InvestmentBlock, as its inner Block converted, if it is in
 *   design mode itself;
 *
 * - as it is, otherwise.
 *
 * \author Francesco Secoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "AbstractPath.h"

#include "Block.h"

#include "Group.h"

#include "InvestmentFunction.h"

#include "SMSnetCDF.h"

#include <algorithm>

#include <filesystem>

#include <iostream>

#include <memory>

#include <numeric>

#include <set>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*----------------------------- CONSTANTS ----------------------------------*/
/*--------------------------------------------------------------------------*/

// the types of the assets [see InvestmentFunction::AssetType]

static constexpr int eUnitBlock = InvestmentFunction::eUnitBlock;
static constexpr int eLine = InvestmentFunction::eLine;
static constexpr int eSecondSize = InvestmentFunction::eSecondSize;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

// whether v is the design Variable of an asset. The classes of the assets
// register it in a group with one of these names, which the classes deriving
// from them inherit (a NuclearUnitBlock has the x_thermal of the
// ThermalUnitBlock it derives from): the name of the group, not the class of
// the Block, tells a design

static bool is_design( const ColVariable * v )
{
 static const std::set< std::string > designs = {
  "x_intermittent" , "x_thermal" , "x_battery" , "x_converter" ,
  "x_network" };

 const auto group = v->get_Group();
 return( group && designs.count( group->get_name() ) );
 }

/*--------------------------------------------------------------------------*/

// the ColVariable that paths reach in block, each with the index of its path;
// a path that reaches none gives a nullptr

static std::vector< std::pair< const ColVariable * , Block::Index > >
 reached( const std::vector< std::unique_ptr< AbstractPath > > & paths ,
          Block * block )
{
 std::vector< std::pair< const ColVariable * , Block::Index > > vars;
 for( Block::Index k = 0 ; k < paths.size() ; ++k ) {
  const auto n = paths[ k ]->get_number_elements< ColVariable >( block );
  for( Block::Index j = 0 ; j < n ; ++j ) {
   const auto v = paths[ k ]->get_element< ColVariable >( block , j );
   vars.emplace_back( v , k );
   if( ! v )  // an empty path has an infinite number of elements
    break;
   }
  }
 return( vars );
 }

/*--------------------------------------------------------------------------*/

// why the scenarios below node, root or a TwoStageStochasticBlock inside it,
// cannot be separated, the empty string if they can. What root makes
// first-stage is the investment, one in every leaf: it has to be made of
// designs, and every node with more than one scenario has to share among
// them exactly the investment, read in the Block its paths are resolved
// against; any other Variable it shares is a decision of a later stage, and
// an investment it does not share is free in all its scenarios but the
// first one

// whether a first-stage AbstractPath of node, or of any
// TwoStageStochasticBlock below it, ties the scenarios: the inner Block of a
// MultiStageStochasticBlock may be one in turn, whose paths tie the scenarios
// below it


/*--------------------------------------------------------------------------*/

// the investment cost and the bounds of a design in the group of a unit
static const std::set< std::string > design_fields = {
 "InvestmentCost" , "MinCapacityDesign" , "MaxCapacityDesign" ,
 "BatteryInvestmentCost" , "BatteryMinCapacityDesign" ,
 "BatteryMaxCapacityDesign" , "ConverterInvestmentCost" ,
 "ConverterMinCapacityDesign" , "ConverterMaxCapacityDesign" };

/*--------------------------------------------------------------------------*/

namespace {

// one design of the inner Block as an asset of the InvestmentBlock

struct DesignAsset {
 Block::Index index;   // of the unit in its UCBlock, or of the line
 int type;             // an InvestmentFunction::AssetType
 double cost;
 double lower;
 double upper;
 int integer;
 bool operator==( const DesignAsset & ) const = default;
 };

/*--------------------------------------------------------------------------*/

// the designs found in a Block, as assets, and the number of its design
// Variable, together with the group of the Block that its first-stage paths,
// if any, are resolved against

struct Designs {
 std::vector< DesignAsset > assets;
 Block::Index number = 0;
 netCDF::NcGroup leaf;
 };

}  // end( unnamed namespace )

/*--------------------------------------------------------------------------*/

// the scalar variable name of g, def if g does not have it

static double scalar( const netCDF::NcGroup & g , const std::string & name ,
                      double def )
{
 ::deserialize( g , def , name );
 return( def );
 }

/*--------------------------------------------------------------------------*/

// the double attribute name of g, def if it has none

static double attribute( const netCDF::NcGroup & g , const std::string & name ,
                         double def )
{
 if( auto att = g.getAtt( name ) ; ! att.isNull() )
  att.getValues( & def );
 return( def );
 }

/*--------------------------------------------------------------------------*/

// the "type" attribute of g, the empty string if it has none

static std::string type_of( const netCDF::NcGroup & g )
{
 std::string type;
 if( auto att = g.getAtt( "type" ) ; ! att.isNull() )
  att.getValues( type );
 return( type );
 }

/*--------------------------------------------------------------------------*/

// whether the group g describes, or holds, a unit or a network in design
// mode: a unit with an investment cost, a DesignNetworkBlock

static bool has_design( const netCDF::NcGroup & g )
{
 const auto type = type_of( g );
 if( type == "DesignNetworkBlock" )
  return( true );
 if( ( type == "IntermittentUnitBlock" ) || ( type == "ThermalUnitBlock" ) ||
     ( type == "NuclearUnitBlock" ) || ( type == "ConversionUnitBlock" ) )
  return( scalar( g , "InvestmentCost" , 0 ) != 0 );
 if( type == "BatteryUnitBlock" )
  return( ( scalar( g , "BatteryInvestmentCost" , 0 ) != 0 ) ||
          ( scalar( g , "ConverterInvestmentCost" , 0 ) != 0 ) );

 for( const auto & sub : g.getGroups() )
  if( has_design( sub.second ) )
   return( true );
 return( false );
 }

/*--------------------------------------------------------------------------*/

// copies into dst the attributes, dimensions, variables and sub-groups of
// src, but those named in skip

static void copy_skipping( const netCDF::NcGroup & src , netCDF::NcGroup dst ,
                        const std::set< std::string > & skip = {} )
{
 for( const auto & att : src.getAtts() )
  netCDF::ncCheck( nc_copy_att( src.getId() , NC_GLOBAL ,
                                att.first.c_str() , dst.getId() ,
                                NC_GLOBAL ) , __FILE__ , __LINE__ );
 for( const auto & [ name , dim ] : src.getDims() )
  if( ! skip.count( name ) )
   dst.addDim( name , dim.isUnlimited() ? 0 : dim.getSize() );
 for( const auto & [ name , var ] : src.getVars() ) {
  if( skip.count( name ) )
   continue;
  if( var.getType() != netCDF::ncString ) {
   netCDF::ncCheck( nc_copy_var( src.getId() , var.getId() , dst.getId() ) ,
                    __FILE__ , __LINE__ );
   continue;
   }
  // nc_copy_var() does not copy strings
  std::vector< netCDF::NcDim > dims;
  std::size_t size = 1;
  for( const auto & dim : var.getDims() ) {
   dims.push_back( dst.getDim( dim.getName() ,
                               netCDF::NcGroup::ParentsAndCurrent ) );
   size *= dim.getSize();
   }
  auto copy = dst.addVar( name , netCDF::ncString , dims );
  for( const auto & att : var.getAtts() )
   netCDF::ncCheck( nc_copy_att( src.getId() , var.getId() ,
                                 att.first.c_str() , dst.getId() ,
                                 copy.getId() ) , __FILE__ , __LINE__ );
  std::vector< char * > strings( size );
  if( size ) {
   var.getVar( strings.data() );
   copy.putVar( const_cast< const char ** >( strings.data() ) );
   nc_free_string( size , strings.data() );
   }
  }
 for( const auto & [ name , sub ] : src.getGroups() )
  if( ! skip.count( name ) )
   copy_skipping( sub , dst.addGroup( name ) );
 }

/*--------------------------------------------------------------------------*/

// the asset of the design of a unit or a line of cost c between lo and hi, a
// negative hi meaning an integer design up to - hi; the asset is fixed at hi,
// with both bounds there, if the bounds leave no choice, or if the design
// costs nothing and widens a line only, where hi is always the best

static DesignAsset design_asset( Block::Index index , int type , double c ,
                                 double lo , double hi , bool widening )
{
 const int integer = ( hi < 0 );
 hi = std::abs( hi );
 lo = std::max( lo , 0.0 );
 if( ( lo >= hi ) || ( ( c == 0 ) && widening ) )
  return( DesignAsset{ index , type , c , hi , hi , 0 } );
 return( DesignAsset{ index , type , c , lo , hi , integer } );
 }

/*--------------------------------------------------------------------------*/

// the designs of the UCBlock described by uc. The asset sets the datum that
// the design multiplies, the kappa constant or the scale factor: the unit has
// to have it at 1. A BatteryUnitBlock has two sizes, the storage and the
// converter, which are a pair of assets [see InvestmentFunction::AssetType],
// a size with no design being fixed at its datum; the converter with neither
// a design nor a "ConverterMaxPower" follows the storage design, and the
// battery is then one asset sizing both. The design of a line is binary if
// its maximum is negative [see DesignNetworkBlock]

static Designs ucblock_designs( const netCDF::NcGroup & uc )
{
 Designs d;
 d.leaf = uc;
 Block::Index units = 0;
 deserialize_dim( uc , "NumberUnits" , units , false );

 for( Block::Index i = 0 ; i < units ; ++i ) {
  const auto g = uc.getGroup( "UnitBlock_" + std::to_string( i ) );
  const auto type = type_of( g );
  if( ! has_design( g ) )
   continue;

  const auto refuse = [ & ]( const std::string & why ) {
   throw( std::logic_error( "design_converter: UnitBlock_" +
                            std::to_string( i ) + ", a " + type + " in "
                            "design mode, " + why ) );
   };
  for( const char * datum : { "Scale" , "Kappa" , "ConverterKappa" } )
   if( scalar( g , datum , 1 ) != 1 )
    refuse( "has a " + std::string( datum ) + " other than 1, which the "
            "investment would overwrite" );
  // the copies of the unit, which serialize() writes as the weight of its
  // Variable [see UnitBlock::replicate()]
  if( attribute( g , "VariableWeight" , 1 ) != 1 )
   refuse( "has a VariableWeight other than 1, i.e., copies, which the "
           "investment would overwrite" );

  if( type == "IntermittentUnitBlock" ) {
   ++d.number;
   d.assets.push_back( design_asset( i , eUnitBlock ,
                                     scalar( g , "InvestmentCost" , 0 ) ,
                                     scalar( g , "MinCapacityDesign" , 0 ) ,
                                     scalar( g , "MaxCapacityDesign" , 1 ) ,
                                     false ) );
   }
  else if( ( type == "ThermalUnitBlock" ) || ( type == "NuclearUnitBlock" ) ) {
   ++d.number;   // the design is binary
   d.assets.push_back( design_asset( i , eUnitBlock ,
                                     scalar( g , "InvestmentCost" , 0 ) ,
                                     0 , -1 , false ) );
   }
  else if( type == "BatteryUnitBlock" ) {
   const auto size = [ & ]( int kind , const std::string & prefix ) {
    const double c = scalar( g , prefix + "InvestmentCost" , 0 );
    if( ! c )
     return( DesignAsset{ i , kind , 0 , 1 , 1 , 0 } );
    ++d.number;
    return( design_asset( i , kind , c ,
                          scalar( g , prefix + "MinCapacityDesign" , 0 ) ,
                          scalar( g , prefix + "MaxCapacityDesign" , 1 ) ,
                          false ) );
    };
   d.assets.push_back( size( eUnitBlock , "Battery" ) );
   if( scalar( g , "ConverterInvestmentCost" , 0 ) ||
       ( ! g.getVar( "ConverterMaxPower" ).isNull() ) )
    d.assets.push_back( size( eSecondSize ,
                              "Converter" ) );
   }
  else
   refuse( "which is not converted: whether copies of the unit, or its "
           "resizing, give the problem of its design is not known" );
  }

 for( const auto & [ name , g ] : uc.getGroups() ) {
  if( type_of( g ) != "DesignNetworkBlock" )
   continue;
  Block::Index networks = 0;
  if( ( name != "NetworkBlock_0" ) ||
      ( ! deserialize_dim( uc , "NumberNetworks" , networks ) ) ||
      ( networks != 1 ) )
   throw( std::logic_error( "design_converter: the "
          "DesignNetworkBlock " + name + " is not the only NetworkBlock of "
          "its UCBlock, which the conversion of the design mode requires" ) );

  Block::Index n = 0 , lines = 0;
  deserialize_dim( g , "NumberDesignLines" , n , false );
  deserialize_dim( uc , "NumberLines" , lines , false );
  // the m values of name in h, a scalar repeated, def if it is not there
  const auto values = []( const netCDF::NcGroup & h , const char * name ,
                          Block::Index m , double def ) {
   std::vector< double > v;
   if( ! ::deserialize( h , name , m , v , true , true ) )
    v.assign( m , def );
   else if( v.size() == 1 )
    v.assign( m , v[ 0 ] );
   return( v );
   };
  const auto cost = values( g , "InvestmentCost" , n , 0 );
  const auto lo = values( g , "MinCapacityDesign" , n , 0 );
  const auto hi = values( g , "MaxCapacityDesign" , n , 1 );
  // the flow limits of each line at each instant, read as DCNetworkBlock
  // reads them; one that is not there is 0
  Block::Index instants = 1;
  deserialize_dim( uc , "NumberInstants" , instants , true );
  boost::multi_array< double , 2 > fmin , fmax;
  ::deserialize( uc , "MinPowerFlow" , { lines , instants } , fmin , true ,
                 true );
  ::deserialize( uc , "MaxPowerFlow" , { lines , instants } , fmax , true ,
                 true );
  // whether the design of line l widens it only: its flow interval holds 0
  // at every instant
  const auto widening = [ & ]( Block::Index l ) {
   for( Block::Index t = 0 ; t < instants ; ++t )
    if( ( fmin.num_elements() && ( fmin[ l ][ t ] > 0 ) ) ||
        ( fmax.num_elements() && ( fmax[ l ][ t ] < 0 ) ) )
     return( false );
   return( true );
   };
  std::vector< Block::Index > line;
  if( ! ::deserialize( g , "DesignLines" , n , line , true , true ) ) {
   line.resize( n );
   std::iota( line.begin() , line.end() , 0 );
   }

  d.number += n;
  for( Block::Index p = 0 ; p < n ; ++p )
   d.assets.push_back( design_asset( line[ p ] , eLine ,
                                     cost[ p ] , lo[ p ] ,
                                     hi[ p ] < 0 ? -1 : hi[ p ] ,
                                     widening( line[ p ] ) ) );
  }

 return( d );
 }

/*--------------------------------------------------------------------------*/

// writes into dst the UCBlock described by uc with its designs d converted:
// the units without their investment cost and bounds, a battery with its
// "ConverterKappa", so that its converter rows exist whatever its sizes, and
// the DesignNetworkBlock replaced by the NetworkBlock of its time instants;
// one that it does not hold is a DCNetworkBlock, which the UCBlock has to
// make too

static void convert_ucblock( const netCDF::NcGroup & uc ,
                             netCDF::NcGroup dst , const Designs & d )
{
 std::set< std::string > skip;
 for( const auto & a : d.assets )
  if( a.type != eLine )
   skip.insert( "UnitBlock_" + std::to_string( a.index ) );
 const auto design_network = uc.getGroup( "NetworkBlock_0" );
 const bool network = ( type_of( design_network ) == "DesignNetworkBlock" );
 if( network ) {
  if( ! uc.getVar( "NetworkConstantTerms" ).isNull() )
   throw( std::logic_error( "design_converter: a UCBlock with a "
          "DesignNetworkBlock and 'NetworkConstantTerms' is not converted" ) );
  skip.insert( { "NumberNetworks" , "NetworkBlock_0" } );
  }

 copy_skipping( uc , dst , skip );

 for( const auto & a : d.assets )
  if( a.type == eUnitBlock ) {
   const auto name = "UnitBlock_" + std::to_string( a.index );
   auto g = dst.addGroup( name );
   copy_skipping( uc.getGroup( name ) , g , design_fields );
   if( ( type_of( g ) == "BatteryUnitBlock" ) &&
       g.getVar( "ConverterKappa" ).isNull() )
    ::serialize( g , "ConverterKappa" , netCDF::NcDouble() , 1.0 );
   }

 if( network ) {
  Block::Index n = 0 , found = 0;
  deserialize_dim( design_network , "NumberSubNetwork" , n , false );
  dst.addDim( "NumberNetworks" , n );
  for( ; found < n ; ++found ) {   // read up to the first missing one
   const auto name = "NetworkBlock_" + std::to_string( found );
   const auto sub = design_network.getGroup( name );
   if( sub.isNull() )
    break;
   copy_skipping( sub , dst.addGroup( name ) );
   }
  std::string classname;
  if( ( found < n ) &&
      ::deserialize( uc , classname , "NetworkBlockClassname" ) &&
      ( classname != "DCNetworkBlock" ) )
   throw( std::logic_error( "design_converter: the "
          "DesignNetworkBlock makes DCNetworkBlock where its UCBlock makes " +
          classname + ", and it is not converted" ) );
  }
 }

/*--------------------------------------------------------------------------*/

// writes into dst, out of the group paths of the first-stage paths of a
// TwoStageStochasticBlock, those that do not reach a design, read in the
// Block described by d.leaf in design mode; each of the d.number designs
// has to be first-stage, since one that is not is decided in each scenario,
// which no asset can be

static void drop_design_paths( const netCDF::NcGroup & paths ,
                               const Designs & d , netCDF::NcGroup dst ,
                               const std::string & where )
{
 std::vector< std::unique_ptr< AbstractPath > > all;
 AbstractPath::vector_deserialize( paths , all );

 std::unique_ptr< Block > leaf( Block::new_Block( d.leaf , nullptr ) );
 if( ! leaf )
  throw( std::logic_error( "design_converter: the Block of the "
                           "scenarios of " + where + " could not be built" ) );
 leaf->generate_abstract_variables();

 std::vector< bool > to_design( all.size() , false );
 std::set< const ColVariable * > first_stage;
 for( const auto & [ v , k ] : reached( all , leaf.get() ) )
  if( v && is_design( v ) ) {
   to_design[ k ] = true;
   first_stage.insert( v );
   }

 if( first_stage.size() != d.number )
  throw( std::logic_error( "design_converter: " + where +
         " makes first-stage " + std::to_string( first_stage.size() ) +
         " of its " + std::to_string( d.number ) + " design Variable: a "
         "design that is not first-stage is decided in each scenario, which "
         "no asset is" ) );

 std::vector< std::unique_ptr< AbstractPath > > kept;
 for( Block::Index k = 0 ; k < all.size() ; ++k )
  if( ! to_design[ k ] )
   kept.push_back( std::move( all[ k ] ) );
 if( ! paths.isNull() ) {
  auto group = dst.addGroup( "StaticAbstractPath" );
  AbstractPath::serialize( kept , group );
  }
 }

/*--------------------------------------------------------------------------*/

// writes into dst the inner Block described by src with its designs
// converted, and returns them. A UCBlock holds them; a StochasticBlock, a
// TwoStageStochasticBlock and a MultiStageStochasticBlock hold UCBlock, which
// all have to have the same designs, and the first-stage paths of the latter
// two are dropped where they reach a design. Designs anywhere else are
// refused

static Designs convert_inner( const netCDF::NcGroup & src ,
                              netCDF::NcGroup dst )
{
 const auto type = type_of( src );

 if( type == "UCBlock" ) {
  auto d = ucblock_designs( src );
  convert_ucblock( src , dst , d );
  return( d );
  }

 if( type == "StochasticBlock" ) {
  copy_skipping( src , dst , { "Block" } );
  return( convert_inner( src.getGroup( "Block" ) , dst.addGroup( "Block" ) ) );
  }

 if( type == "TwoStageStochasticBlock" ) {
  copy_skipping( src , dst , { "StochasticBlock" , "StaticAbstractPath" } );
  auto d = convert_inner( src.getGroup( "StochasticBlock" ) ,
                          dst.addGroup( "StochasticBlock" ) );
  drop_design_paths( src.getGroup( "StaticAbstractPath" ) , d , dst , type );
  return( d );
  }

 if( type == "MultiStageStochasticBlock" ) {
  Block::Index n = 0;
  deserialize_dim( src , "NumberSubBlocks" , n , false );
  std::set< std::string > skip = { "StaticAbstractPath" };
  for( Block::Index l = 0 ; l < n ; ++l )
   skip.insert( "Block_" + std::to_string( l ) );
  copy_skipping( src , dst , skip );

  Designs d;
  for( Block::Index l = 0 ; l < n ; ++l ) {
   const auto name = "Block_" + std::to_string( l );
   auto d_l = convert_inner( src.getGroup( name ) , dst.addGroup( name ) );
   if( ! l )
    d = std::move( d_l );
   else
    if( d_l.assets != d.assets )
     throw( std::logic_error( "design_converter: " + name +
                              " of the " + type + " has designs other than "
                              "those of Block_0" ) );
   }
  drop_design_paths( src.getGroup( "StaticAbstractPath" ) , d , dst , type );
  return( d );
  }

 if( has_design( src ) )
  throw( std::logic_error( "design_converter: a " + type +
                           " holds designs, which are not converted" ) );
 copy_skipping( src , dst );
 return( Designs() );
 }

/*--------------------------------------------------------------------------*/

// writes into dst the InvestmentBlock described by group with its inner
// Block in design mode converted: each design is an asset, its fields at the
// root. The designs have no ColVariable of the root to be bound to but new
// ones, so they are converted only in the inner Block at the root of a group
// that declares no asset

static void convert_design_mode( const netCDF::NcGroup & group ,
                                 netCDF::NcGroup dst )
{
 if( ! group.getGroup( "Component_0" ).isNull() )
  throw( std::logic_error( "design_converter: an inner Block in "
         "design mode is converted only at the root, not in a Component_<k>, "
         "whose designs would be bound to no ColVariable of the root" ) );
 Block::Index num_assets = 0;
 deserialize_dim( group , "NumAssets" , num_assets );
 if( num_assets )
  throw( std::logic_error( "design_converter: the inner Block is "
         "in design mode, and the root declares assets too: the designs are "
         "converted only into the assets of a root that declares none" ) );

 copy_skipping( group , dst , { "InnerBlock" , "NumAssets" , "Assets" ,
                             "AssetType" , "Cost" , "LowerBound" ,
                             "UpperBound" , "Integer" } );
 const auto d = convert_inner( group.getGroup( "InnerBlock" ) ,
                               dst.addGroup( "InnerBlock" ) );

 const auto dim = dst.addDim( "NumAssets" , d.assets.size() );
 std::vector< Block::Index > index;
 std::vector< int > type , integer;
 std::vector< double > cost , lower , upper;
 for( const auto & a : d.assets ) {
  index.push_back( a.index );
  type.push_back( a.type );
  cost.push_back( a.cost );
  lower.push_back( a.lower );
  upper.push_back( a.upper );
  integer.push_back( a.integer );
  }
 ::serialize( dst , "Assets" , netCDF::NcUint() , dim , index );
 ::serialize( dst , "AssetType" , netCDF::NcInt() , dim , type );
 ::serialize( dst , "Cost" , netCDF::NcDouble() , dim , cost );
 ::serialize( dst , "LowerBound" , netCDF::NcDouble() , dim , lower );
 ::serialize( dst , "UpperBound" , netCDF::NcDouble() , dim , upper );
 if( std::count( integer.begin() , integer.end() , 1 ) )
  ::serialize( dst , "Integer" , netCDF::NcInt() , dim , integer );
 }

/*--------------------------------------------------------------------------*/

// writes into dst the Block described by src: converted if it is an
// InvestmentBlock whose inner Block is in design mode, inside a new
// InvestmentBlock if it is in design mode itself, as it is otherwise; true
// if it is converted

static bool convert_Block( const netCDF::NcGroup & src , netCDF::NcGroup dst )
{
 if( type_of( src ) == "InvestmentBlock" ) {
  // the design mode is in its InnerBlock, or in that of a Component_<k>,
  // which convert_design_mode() refuses
  if( ! has_design( src ) ) {
   copy_skipping( src , dst );
   return( false );
   }
  convert_design_mode( src , dst );
  return( true );
  }

 if( ! has_design( src ) ) {
  copy_skipping( src , dst );
  return( false );
  }

 // the Solver of a Block does not apply the weight of its Objective, which
 // the InvestmentBlock wrapping it would
 double weight = 1;
 if( auto att = src.getAtt( "ObjectiveWeight" ) ; ! att.isNull() )
  att.getValues( & weight );
 if( weight != 1 )
  throw( std::logic_error( "design_converter: the Block has an "
                           "'ObjectiveWeight' other than 1, which the "
                           "InvestmentBlock would apply and its Solver "
                           "does not" ) );

 // an InvestmentBlock with src as its inner Block and no asset, in memory,
 // which is then converted as any other
 netCDF::NcFile wrap;
 wrap.create( "design_converter_wrap.nc" ,
              NC_NETCDF4 | NC_DISKLESS | NC_CLOBBER );
 wrap.putAtt( "type" , "InvestmentBlock" );
 copy_skipping( src , wrap.addGroup( "InnerBlock" ) );
 convert_design_mode( wrap , dst );
 return( true );
 }

/*--------------------------------------------------------------------------*/
/*-------------------------------- MAIN ------------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 if( argc != 3 ) {
  std::cerr << "Usage: " << argv[ 0 ] << " <input file> <output file>"
            << std::endl;
  return( 1 );
  }

 // the output is written aside, and renamed when all is done: a refused
 // conversion leaves no file
 const std::string part = std::string( argv[ 2 ] ) + ".part";

 try {
  netCDF::NcFile in( argv[ 1 ] , netCDF::NcFile::read );

  int file_type = eBlockFile;
  if( auto att = in.getAtt( "SMS++_file_type" ) ; ! att.isNull() )
   att.getValues( & file_type );
  if( ( file_type != eBlockFile ) && ( file_type != eProbFile ) )
   throw( std::invalid_argument( "design_converter: " +
                                 std::string( argv[ 1 ] ) + " is neither a "
                                 "Block file nor a problem file" ) );

  int converted = 0;
  {
   netCDF::NcFile out( part , netCDF::NcFile::replace ,
                       netCDF::NcFile::nc4 );
   for( const auto & att : in.getAtts() )
    netCDF::ncCheck( nc_copy_att( in.getId() , NC_GLOBAL ,
                                  att.first.c_str() , out.getId() ,
                                  NC_GLOBAL ) , __FILE__ , __LINE__ );

   // in a Block file each child group is a Block, in a problem file the
   // child "Block" of each child group is
   for( const auto & [ name , group ] : in.getGroups() ) {
    auto dst = out.addGroup( name );
    if( file_type == eBlockFile )
     converted += convert_Block( group , dst );
    else {
     copy_skipping( group , dst , { "Block" } );
     if( auto block = group.getGroup( "Block" ) ; ! block.isNull() )
      converted += convert_Block( block , dst.addGroup( "Block" ) );
     }
    }
   }
  std::filesystem::rename( part , argv[ 2 ] );

  std::cout << converted << " Block converted" << std::endl;
  }
 catch( const std::exception & e ) {
  std::error_code ec;
  std::filesystem::remove( part , ec );
  std::cerr << e.what() << std::endl;
  return( 1 );
  }

 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*---------------------- End File design_converter.cpp ---------------------*/
/*--------------------------------------------------------------------------*/
