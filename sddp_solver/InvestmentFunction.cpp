/*--------------------------------------------------------------------------*/
/*---------------------- File InvestmentFunction.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of the InvestmentFunction class.
 *
 * \author Rafael Durbano Lobato \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Rafael Durbano Lobato.
 */
/*--------------------------------------------------------------------------*/
/*---------------------------- IMPLEMENTATION ------------------------------*/
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "BatteryUnitBlock.h"
#include "BendersBFunction.h"
#include "BendersBlock.h"
#include "BlockSolverConfig.h"
#include "DCNetworkBlock.h"
#include "FRealObjective.h"
#include "Observer.h"
#include "OneVarConstraint.h"
#include "RBlockConfig.h"
#include "IntermittentUnitBlock.h"
#include "InvestmentFunction.h"
#include "SDDPBlock.h"
#include "SDDPGreedySolver.h"
#include "SMSTypedefs.h"
#include "StochasticBlock.h"
#include "ThermalUnitBlock.h"
#include "UCBlock.h"

#include <cmath>
#include <functional>
#include <queue>

/*--------------------------------------------------------------------------*/
/*------------------------- NAMESPACE AND USING ----------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*----------------------------- STATIC MEMBERS -----------------------------*/
/*--------------------------------------------------------------------------*/

// register InvestmentFunction to the Block factory

SMSpp_insert_in_factory_cpp_1( InvestmentFunction );

/*--------------------------------------------------------------------------*/
/*---------------------------------TODO-------------------------------------*/
/*--------------------------------------------------------------------------*/

void InvestmentFunction::load( std::istream &input ) {
 throw( std::logic_error( "InvestmentFunction::load(): "
                          "not implemented yet." ) );
}

/*--------------------------------------------------------------------------*/
/*------------ CONSTRUCTING AND DESTRUCTING InvestmentFunction -------------*/
/*--------------------------------------------------------------------------*/

InvestmentFunction::InvestmentFunction
( Block * inner_block , VarVector && x , IndexVector && block_indices ,
  IndexVector && line_indices , RealVector && linear_coefficients ,
  Observer * const observer )
 : C05Function( observer ) , f_blocks_are_updated( false ) ,
   f_solver_status( kUnEval ) , f_diagonal_linearization_required( false ) ,
   f_id( this ) {

 set_inner_block( inner_block );
 set_variables( std::move( x ) );

 v_block_indices = std::move( block_indices );
 v_line_indices  = std::move( line_indices );
 v_linear_coefficients  = std::move( linear_coefficients );
}

/*--------------------------------------------------------------------------*/

InvestmentFunction::~InvestmentFunction() {
 if( ! v_Block.empty() ) {
  assert( v_Block.size() == 1 );
  delete v_Block.front();
 }
}

/*--------------------------------------------------------------------------*/

void InvestmentFunction::deserialize( const netCDF::NcGroup & group ,
                                      ModParam issueMod ) {

 // Deserialize the dimensions

 Index num_unit_blocks = 0;
 Index num_lines = 0;

 ::deserialize_dim( group , "NumUnitBlocks" , num_unit_blocks );
 ::deserialize_dim( group , "NumLines" , num_lines );

 // Deserialize the UnitBlock and line indices.

 if( num_unit_blocks )
  ::deserialize( group , "BlockIndices" , num_unit_blocks ,
                 v_block_indices , false );

 if( num_lines )
  ::deserialize( group , "LineIndices" , num_lines , v_line_indices , false );

 const auto num_var = num_unit_blocks + num_lines;

 if( ! v_x.empty() ) {
  if( num_var != v_x.size() )
   throw std::logic_error( "InvestmentFunction::deserialize: the number of "
                           "assets to invest (" + std::to_string( num_var ) +
                           ") is different from the number of active variables"
                           "(" + std::to_string( v_x.size() ) + ")." );
 }

 // Deserialize the linear coeffients of the objective function

 if( ::deserialize( group , "LinearCoefficients" , num_var ,
                    v_linear_coefficients , true , true ) ) {
  if( v_linear_coefficients.size() == 1 )
   v_linear_coefficients.resize( num_var , v_linear_coefficients.front() );
 }
 else {
  // All coefficients are zero.
  v_linear_coefficients.resize( num_var , 0 );
 }

 // Deserialize the inner Block

 auto inner_block_group = group.getGroup( BLOCK_NAME );
 if( inner_block_group.isNull() )
  throw std::logic_error( "InvestmentFunction::deserialize: the '" +
                          BLOCK_NAME + "' group must be present." );

 auto inner_block = new_Block( inner_block_group , this );
 if( ! inner_block )
  throw std::logic_error( "InvestmentFunction::deserialize: the '" +
                          BLOCK_NAME + "' group is present "
                          "but its description is incomplete." );

 set_inner_block( inner_block );

 Block::deserialize( group );

}  // end( InvestmentFunction::deserialize )

/*--------------------------------------------------------------------------*/
/*-------------------------- OTHER INITIALIZATIONS -------------------------*/
/*--------------------------------------------------------------------------*/

void InvestmentFunction::set_variables( VarVector && x ) {
 if( ! v_linear_coefficients.empty() )
  if( v_linear_coefficients.size() != x.size() )
   throw( std::logic_error("InvestmentFunction::set_variables: given x has "
                           "size " + std::to_string( x.size() ) + ", but the "
                           "number of linear coefficients is " +
                           std::to_string( v_linear_coefficients.size() ) ) );

 v_x = std::move( x );
 f_blocks_are_updated = false;
}  // end( InvestmentFunction::set_variables )

/*--------------------------------------------------------------------------*/
/*---- METHODS FOR HANDLING "ACTIVE" Variable IN THE InvestmentFunction ----*/
/*--------------------------------------------------------------------------*/

void InvestmentFunction::map_active( c_Vec_p_Var & vars , Subset & map ,
                                     const bool ordered ) const {
 if( v_x.empty() )
  return;

 if( map.size() < vars.size() )
  map.resize( vars.size() );

 if( ordered ) {
  Index found = 0;
  for( Index i = 0 ; i < v_x.size() ; ++i ) {
   auto itvi = std::lower_bound( vars.begin() , vars.end() , v_x[ i ] );
   if( itvi != vars.end() ) {
    map[ std::distance( vars.begin() , itvi ) ] = i;
    ++found;
   }
  }
  if( found < vars.size() )
   throw( std::invalid_argument( "InvestmentFunction::map_active: some Variable "
                                 "is not active." ) );
 }
 else {
  auto it = map.begin();
  for( auto var : vars ) {
   auto i = this->is_active( var );
   if( i >= v_x.size() )
    throw( std::invalid_argument( "InvestmentFunction::map_active: some Variable "
                                  "is not active" ) );
   *(it++) = i;
  }
 }
}  // end( InvestmentFunction::map_active )

/*--------------------------------------------------------------------------*/
/*-------------- METHODS FOR MODIFYING THE InvestmentFunction --------------*/
/*--------------------------------------------------------------------------*/

void InvestmentFunction::remove_variable( Index i , ModParam issueMod ) {
 if( i >= v_x.size() )
  throw( std::logic_error( "InvestmentFunction::remove_variable: invalid "
                           "Variable index " + std::to_string( i ) + "." ) );

 auto var = v_x[ i ];
 v_x.erase( v_x.begin() + i );    // erase it in v_x

 if( i < v_block_indices.size() )
  // The Variable being removed is associated with an UnitBlock
  v_block_indices.erase( v_block_indices.begin() + i );
 else
  // The Variable being removed is associated with a transmission line
  v_line_indices.erase( v_line_indices.begin() + i - v_block_indices.size() );

 // Erase the linear coefficient associated with the Variable being removed
 v_linear_coefficients.erase( v_linear_coefficients.begin() + i );

 f_blocks_are_updated = false;

 if( ( ! f_Observer ) || ( ! f_Observer->issue_mod( issueMod ) ) )
  return;

 // Now issue the Modification.
 // An InvestmentFunction is strongly quasi-additive.
 f_Observer->add_Modification( std::make_shared<C05FunctionModVarsRngd>
                               ( this , Vec_p_Var( { var } ) ,
                                 Range( i , i + 1 ) , 0 ,
                                 Observer::par2concern( issueMod ) ) ,
                               Observer::par2chnl( issueMod ) );

}  // end( InvestmentFunction::remove_variable( index ) )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::remove_variables( Range range , ModParam issueMod ) {

 range.second = std::min( range.second , Index( v_x.size() ) );
 if( range.second <= range.first )
  return;

 f_blocks_are_updated = false;

 if( ( range.first == 0 ) && ( range.second == Index( v_x.size() ) ) ) {
  // removing *all* Variables
  Vec_p_Var vars( v_x.size() );

  for( decltype( v_x )::size_type i = 0 ; i < v_x.size() ; ++i )
   vars[ i ] = v_x[ i ];

  v_x.clear();
  v_block_indices.clear();
  v_line_indices.clear();
  v_linear_coefficients.clear();

  // Now issue the Modification.
  // An InvestmentFunction is strongly quasi-additive.
  if( f_Observer && f_Observer->issue_mod( issueMod ) )
   f_Observer->add_Modification( std::make_shared<C05FunctionModVarsRngd>
                                 ( this , std::move( vars ) , range , 0 ,
                                   Observer::par2concern( issueMod ) ) ,
                                 Observer::par2chnl( issueMod ) );
  return;
 }

 // Removing *some* Variables.

 // Iterators to the Variables to be removed.

 const auto v_x_it = std::make_pair( v_x.begin() + range.first ,
                                     v_x.begin() + range.second );

 const auto erase = [ this , &v_x_it , range ]() {

  using size_type = decltype( v_block_indices.size() );

  // Range of UnitBlocks being removed
  const auto range_blocks = std::make_pair
   ( std::min( v_block_indices.size() , size_type{ range.first } ) ,
     std::min( v_block_indices.size() , size_type{ range.second } ) );

  // Range of lines being removed
  const auto range_lines = std::make_pair
   ( std::max( v_block_indices.size() , size_type{ range.first } ) -
     v_block_indices.size() ,
     std::max( v_block_indices.size() , size_type{ range.second } ) -
     v_block_indices.size() );

  // Iterators to the elements to be removed

  const auto v_block_indices_it =
   std::make_pair( v_block_indices.begin() + range_blocks.first ,
                   v_block_indices.begin() + range_blocks.second );

  const auto v_line_indices_it =
   std::make_pair( v_line_indices.begin() + range_lines.first ,
                   v_line_indices.begin() + range_lines.second );

  const auto v_linear_coefficients_it =
   std::make_pair( v_linear_coefficients.begin() + range.first ,
                   v_linear_coefficients.begin() + range.second );

  v_x.erase( v_x_it.first , v_x_it.second );
  v_block_indices.erase( v_block_indices_it.first , v_block_indices_it.second );
  v_line_indices.erase( v_line_indices_it.first , v_line_indices_it.second );
  v_linear_coefficients.erase( v_linear_coefficients_it.first ,
                               v_linear_coefficients_it.second );
 };

 if( f_Observer && f_Observer->issue_mod( issueMod ) ) {
  // Somebody is there: meanwhile, prepare data for the Modification

  Vec_p_Var vars( range.second - range.first );
  std::copy( v_x_it.first , v_x_it.second , vars.begin() );

  // Erase the elements associated with the Variables being removed
  erase();

  // Now issue the Modification.
  // An InvestmentFunction is strongly quasi-additive
  f_Observer->add_Modification( std::make_shared<C05FunctionModVarsRngd>
                                ( this , std::move( vars ) , range , 0 ,
                                  Observer::par2concern( issueMod ) ) ,
                                Observer::par2chnl( issueMod ) );
 }
 else  // no one is there: just do it
  // Erase the elements associated with the Variables being removed
  erase();

}  // end( InvestmentFunction::remove_variables( range ) )

/*--------------------------------------------------------------------------*/

template< class T >
static void compact( std::vector< T > & x ,
                     const InvestmentFunction::Subset & indices ) {

 InvestmentFunction::Index i = indices.front();
 auto xit = x.begin() + (i++);
 for( auto nit = ++(indices.begin()) ; nit != indices.end() ; ++i )
  if( *nit == i )
   ++nit;
  else
   *(xit++) = std::move( x[ i ] );

 for( ; i < x.size() ; ++i )
  *(xit++) = std::move( x[ i ] );

 x.resize( x.size() - indices.size() );
}

/*--------------------------------------------------------------------------*/

void InvestmentFunction::remove_variables( Subset && indices , bool ordered ,
                                           ModParam issueMod ) {

 if( indices.empty() ) {      // removing *all* Variables

  if( v_x.empty() )       // there is no Variable to be removed
   return;                // cowardly (and silently) return

  Vec_p_Var vars( v_x.size() );

  for( Index i = 0 ; i < v_x.size() ; ++i )
   vars[ i ] = v_x[ i ];

  // Clear all elements
  v_x.clear();
  v_block_indices.clear();
  v_line_indices.clear();
  v_linear_coefficients.clear();

  f_blocks_are_updated = false;

  // Now issue the Modification: note that the subset is empty.
  // An InvestmentFunction is strongly quasi-additive, and indices is ordered.
  if( f_Observer && f_Observer->issue_mod( issueMod ) )
   f_Observer->add_Modification( std::make_shared<C05FunctionModVarsSbst>
                                 ( this , std::move( vars ) , Subset() , true ,
                                   0 , Observer::par2concern( issueMod ) ) ,
                                 Observer::par2chnl( issueMod ) );
  return;
 }

 // removing *some* Variables

 if( ! ordered )
  std::sort( indices.begin() , indices.end() );

 if( indices.back() >= v_x.size() )  // the last name is wrong
  throw( std::invalid_argument( "InvestmentFunction::remove_variables: wrong "
                                "Variable index in the Subset indices." ) );

 f_blocks_are_updated = false;

 const auto erase = [ this , &indices ]() {
  Subset blocks_to_remove, lines_to_remove;
  blocks_to_remove.reserve( indices.size() );
  lines_to_remove.reserve( indices.size() );
  for( auto i : indices ) {
   if( i < v_block_indices.size() )
    blocks_to_remove.push_back( i );
   else
    lines_to_remove.push_back( i - v_block_indices.size() );
  }

  compact( v_block_indices , blocks_to_remove );
  compact( v_line_indices , lines_to_remove );
  compact( v_linear_coefficients , indices );
  compact( v_x , indices );
 };

 if( f_Observer && f_Observer->issue_mod( issueMod ) ) {
  Vec_p_Var vars( indices.size() );
  auto its = vars.begin();
  for( auto nm : indices )
   *(its++) = v_x[ nm ];

  erase();

  // Remove

  // Now issue the Modification.
  // An InvestmentFunction is strongly quasi-additive, and indices is ordered.
  f_Observer->add_Modification( std::make_shared<C05FunctionModVarsSbst>
                                ( this , std::move( vars ) ,
                                  std::move( indices ) , true , 0 ,
                                  Observer::par2concern( issueMod ) ) ,
                                Observer::par2chnl( issueMod ) );
 }
 else  // no one is there: just do it
  erase();

}  // end( InvestmentFunction::remove_variables( subset ) )

/*--------------------------------------------------------------------------*/
/*------------ METHODS FOR Saving THE DATA OF THE InvestmentFunction -------*/
/*--------------------------------------------------------------------------*/

void InvestmentFunction::serialize( netCDF::NcGroup & group ) const {

 Block::serialize( group );

 auto NumUnitBlocks = group.addDim( "NumUnitBlocks" , v_block_indices.size() );
 auto NumLines = group.addDim( "NumLines" , v_line_indices.size() );

 const auto num_var = NumUnitBlocks.getSize() + NumLines.getSize();
 auto NumVar = group.addDim( "NumVar" , num_var );

 ::serialize( group , "BlockIndices" , netCDF::NcUint() , NumUnitBlocks ,
              v_block_indices );

 ::serialize( group , "LineIndices" , netCDF::NcUint() , NumLines ,
              v_line_indices );

 ::serialize( group , "LinearCoefficients" , netCDF::NcDouble() , NumVar ,
              v_linear_coefficients );

 if( auto inner_block = get_inner_block() ) {
  auto inner_block_group = group.addGroup( BLOCK_NAME );
  inner_block->serialize( inner_block_group );
 }
}

/*--------------------------------------------------------------------------*/
/*-------- METHODS DESCRIBING THE BEHAVIOR OF THE InvestmentFunction -------*/
/*--------------------------------------------------------------------------*/

int InvestmentFunction::compute( bool changedvars ) {

 if( ( ! changedvars ) && f_blocks_are_updated )
  // TODO We need another flag telling whether the sub-Block has changed since
  // the last call.
  return( f_solver_status ); //  nothing changed since last call, nothing to do

 if( v_Block.size() != 1 )
  throw( std::logic_error( "InvestmentFunction::compute: there must be exactly "
                           "one sub-Block, but there is (are) " +
                           std::to_string( v_Block.size() ) + "." ) );

 auto solver = get_solver();

 if( ! solver )
  throw( std::logic_error
         ( "InvestmentFunction::compute: no Solver attached to sub-Block" ) );

 if( generator_node_map.empty() )
  build_generator_node_map();

 if( changedvars || ( ! f_blocks_are_updated ) ) {
  // update the Blocks

  // try to lock the inner Block: if this does not work
  auto owned = v_Block.front()->is_owned_by( f_id );
  if( ( ! owned ) && ( ! v_Block.front()->lock( f_id ) ) )
   return( kError );     // that's clearly an error

  update_blocks();

  if( ! owned )
   v_Block.front()->unlock( f_id );  // unlock the inner Block
 }

 const auto sddp_block = static_cast< SDDPBlock * >( v_Block.front() );
 const auto num_scenarios = sddp_block->get_scenario_set().size();
 f_value = 0.0;
 reset_linearization();

 for( int scenario = 0 ; scenario < num_scenarios ; ++scenario ) {
  solver->set_par( SDDPGreedySolver::intScenarioId , scenario );
  f_solver_status = solver->compute( true );

  if( ! solver->has_var_solution() )
   return( f_solver_status );

  solver->get_var_solution();

  f_value += solver->get_var_value();

  update_linearization();
 }

 // Compute the expectation of the subgradients

 for( Index i = 0 ; i < v_linearization.size() ; ++i ) {
  v_linearization[ i ] /= num_scenarios;
 }

 // Consider the linear term of the objective

 for( Index i = 0 ; i < v_linear_coefficients.size() ; ++i ) {

  // Update the objective value
  f_value += v_linear_coefficients[ i ] * v_x[ i ]->get_value();

  // Update the linearization
  v_linearization[ i ] += v_linear_coefficients[ i ];
 }

 return( f_solver_status );

}  // end( InvestmentFunction::compute )

/*--------------------------------------------------------------------------*/

static RealObjective::OFValue get_recours_obj( const Block * blck ) {
 RealObjective::OFValue rv = 0;
 if( auto obj = dynamic_cast< RealObjective * >( blck->get_objective() ) )
  rv = obj->get_constant_term();
 for( const auto bk : blck->get_nested_Blocks() )
  rv += get_recours_obj( bk );

 return( rv );
};

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

Function::FunctionValue InvestmentFunction::get_constant_term( void ) const {
 if( auto bk = get_inner_block() )
  return( get_recours_obj( bk ) );
 else
  return( 0 );
}

/*--------------------------------------------------------------------------*/

bool InvestmentFunction::is_convex( void ) const {
 if( v_Block.empty() )
  return false;
 return( get_objective_sense() == Objective::eMin );
}

/*--------------------------------------------------------------------------*/

bool InvestmentFunction::is_concave( void ) const {
 if( v_Block.empty() )
  return false;
 return( get_objective_sense() == Objective::eMax );
}

/*--------------------------------------------------------------------------*/

bool InvestmentFunction::has_linearization( const bool diagonal ) {

 auto solver = get_solver();

 if( ! solver )
  return false;

 if( diagonal ) {
  f_diagonal_linearization_required = true;
  return solver->has_var_solution();
 }
 else {
  throw( std::logic_error( "InvestmentFunction::has_linearization: vertical "
                           "linearization not implemented yet." ) );
 }
}  // end( InvestmentFunction::has_linearization )

/*--------------------------------------------------------------------------*/

Function::FunctionValue InvestmentFunction::get_value( void ) const {
 auto solver = get_solver();
 if( solver->has_var_solution() )
  return f_value;
 if( get_objective_sense() == Objective::eMin )
  return Inf< double >();
 return -Inf< double >();
} // end ( InvestmentFunction::get_value )

/*--------------------------------------------------------------------------*/

int InvestmentFunction::get_objective_sense() const {
 auto inner_block = get_ucblock( 0 );
 assert( inner_block );
 return inner_block->get_objective_sense();
}

/*--------------------------------------------------------------------------*/

UCBlock * InvestmentFunction::get_ucblock( Index stage ) const {
 auto benders_function = get_benders_function( stage );
 assert( benders_function );
 return dynamic_cast< UCBlock * >( benders_function->get_inner_block() );
}

/*--------------------------------------------------------------------------*/

CDASolver * InvestmentFunction::get_ucblock_solver( Index stage ) const {
 if( auto ucblock = get_ucblock( stage ) )
  if( ! ucblock->get_registered_solvers().empty() )
   return
    dynamic_cast< CDASolver * > ( ucblock->get_registered_solvers().front() );
 return nullptr;
}

/*--------------------------------------------------------------------------*/

BendersBFunction *
InvestmentFunction::get_benders_function( Index stage ) const {

 auto sddp_block = static_cast< SDDPBlock * >( v_Block.front() );
 if( stage >= sddp_block->get_time_horizon() )
  throw( std::invalid_argument( "InvestmentFunction::get_benders_function: "
                                "invalid stage index: " +
                                std::to_string( stage ) ) );

 auto benders_block = static_cast< BendersBlock * >
  ( sddp_block->get_sub_Block( stage )->get_inner_block() );

 auto objective = static_cast< FRealObjective * >
  ( benders_block->get_objective() );

 return static_cast< BendersBFunction * >( objective->get_function() );
}

/*--------------------------------------------------------------------------*/

void InvestmentFunction::reset_linearization() {
 v_linearization.resize( v_x.size() );
 std::fill_n( v_linearization.begin() , v_x.size() , 0 );
}

/*--------------------------------------------------------------------------*/

Index InvestmentFunction::get_node( Index stage , Index i ,
                                    Index generator ) const {
 // i is between 0 and v_block_indices.size() - 1.
 return generator_node_map[ stage ][ i ][ generator ];
}

/*--------------------------------------------------------------------------*/

void InvestmentFunction::build_generator_node_map() {

 const auto sddp_block = static_cast< SDDPBlock * >( v_Block.front() );
 const auto num_stages = sddp_block->get_time_horizon();

 generator_node_map.resize( num_stages );

 for( Index stage = 0 ; stage < num_stages ; ++stage ) {

  generator_node_map[ stage ].resize( v_block_indices.size() );

  const auto ucblock = get_ucblock( stage );
  const auto network_data = ucblock->get_NetworkData();
  const auto number_nodes = network_data ? network_data->get_number_nodes() : 1;

  if( number_nodes <= 1 ) {
   // Since there is only one node, all generators belong to the same node
   // (node 0).
   for( Index i = 0 ; i < v_block_indices.size() ; ++i ) {
    const auto unit_block = ucblock->get_unit_block( v_block_indices[ i ] );
    const auto num_generators = unit_block->get_number_generators();
    generator_node_map[ stage ][ i ].resize( num_generators , 0 );
   }
   continue;
  }

  const auto number_units = ucblock->get_number_units();
  const auto & generator_node = ucblock->get_generator_node();

  for( Index node_id = 0 ; node_id < number_nodes ; ++node_id ) {

   Index elc_generator = 0;
   for( Index unit_id = 0 ; unit_id < number_units ; unit_id++ ) {

    const auto unit_block = ucblock->get_unit_block( unit_id );
    const auto num_generators = unit_block->get_number_generators();

    auto it = std::find( v_block_indices.cbegin() ,
                         v_block_indices.cend() , unit_id );

    const auto index = std::distance( v_block_indices.cbegin() , it );

    if( index == v_block_indices.size() ) {
     // This UnitBlock is not subject to investment.
     elc_generator += num_generators;
     continue;
    }

    generator_node_map[ stage ][ index ].resize( num_generators );

    for( Index generator = 0 ; generator < num_generators ;
         ++generator , ++elc_generator ) {
     generator_node_map[ stage ][ index ][ generator ] =
      generator_node[ elc_generator ];
    }
   } // end( for each UnitBlock )
  } // end( for each node )
 } // end( for each stage )
} // end( InvestmentFunction::build_generator_node_map )

/*--------------------------------------------------------------------------*/

double InvestmentFunction::compute_scale_linearization( Index i ,
                                                        Index stage ) {

 /* TODO The following code does not take into account the pollutant budget
  * constraints and the heat constraints. When these constraints are correctly
  * implemented, this function must be updated. */

 const auto ucblock = get_ucblock( stage );
 const auto network_data = ucblock->get_NetworkData();
 const auto number_nodes = network_data ? network_data->get_number_nodes() : 1;
 const auto time_horizon = ucblock->get_time_horizon();

 const auto block = ucblock->get_unit_block( v_block_indices[ i ] );

 // This is the contribution to the linearization associated with this
 // UnitBlock.
 double linearization = 0;

 // Add the contribution associated with the node injection constraints

 const auto & node_injection_constraints =
  ucblock->get_node_injection_constraints();

 for( Index t = 0 ; t < time_horizon ; ++t ) {

  for( Index g = 0 ; g < block->get_number_generators() ; ++g ) {

   const auto node = get_node( stage , i , g );
   const auto dual = node_injection_constraints[ t ][ node ].get_dual();
   const auto active_power = block->get_active_power( g )[ t ].get_value();
   linearization += dual * active_power;

   if( auto fc = block->get_fixed_consumption( g ) ) {
    if( auto u = block->get_commitment( g ) ) {
     const auto commitment = u[ t ].get_value();
     const auto fixed_consumption = fc[ t ];
     linearization += dual * fixed_consumption * ( 1.0 - commitment );
    }
   }

  } // end( for each generator )
 } // end( for each time instant )

 // Add the contribution associated with the primary demand constraints.

 const auto & primary_demand_constraints =
  ucblock->get_primary_demand_constraints();

 if( ! primary_demand_constraints.empty() ) {

  const auto number_primary_zones = ucblock->get_number_primary_zones();

  for( Index t = 0 ; t < time_horizon ; ++t ) {
   for( Index zone_id = 0 ; zone_id < number_primary_zones ; ++zone_id ) {
    for( Index node_id = 0 ; node_id < number_nodes ; ++node_id ) {
     if( ! ucblock->node_belongs_to_primary_zone( node_id , zone_id ) )
      continue;

     // Compute the index of the first electrical generator of the current
     // UnitBlock.
     Index elc_generator = 0;
     for( Index unit_id = 0 ; unit_id < v_block_indices[ i ] ; ++unit_id ) {
      const auto unit_block = ucblock->get_unit_block( unit_id );
      elc_generator += unit_block->get_number_generators();
     }

     const auto num_generators = block->get_number_generators();

     for( Index generator = 0 ; generator < num_generators ;
          ++generator , ++elc_generator ) {

      if( ! ucblock->generator_belongs_to_node( elc_generator , node_id ) )
       continue;

      if( const auto primary_s_r =
          block->get_primary_spinning_reserve( generator ) ) {

       const auto primary_spinning_reserve = & primary_s_r[ t ];
       const auto dual = primary_demand_constraints[ t ][ zone_id ].get_dual();
       linearization += - dual * primary_spinning_reserve->get_value();
      }

     } // end( for each generator )
    } // end( for each node )
   } // end( for each zone )
  } // end( for each time instant )

 } // end( non-empty primary demand constraints )

 // Add the contribution associated with the secondary demand constraints.

 const auto & secondary_demand_constraints =
  ucblock->get_secondary_demand_constraints();

 if( ! secondary_demand_constraints.empty() ) {

  const auto number_secondary_zones = ucblock->get_number_secondary_zones();

  for( Index t = 0 ; t < time_horizon ; ++t ) {
   for( Index zone_id = 0 ; zone_id < number_secondary_zones ; ++zone_id ) {
    for( Index node_id = 0 ; node_id < number_nodes ; ++node_id ) {
     if( ! ucblock->node_belongs_to_secondary_zone( node_id , zone_id ) )
      continue;

     // Compute the index of the first electrical generator of the current
     // UnitBlock.
     Index elc_generator = 0;
     for( Index unit_id = 0 ; unit_id < v_block_indices[ i ] ; ++unit_id ) {
      const auto unit_block = ucblock->get_unit_block( unit_id );
      elc_generator += unit_block->get_number_generators();
     }

     const auto num_generators = block->get_number_generators();

     for( Index generator = 0 ; generator < num_generators ;
          ++generator , ++elc_generator ) {

      if( ! ucblock->generator_belongs_to_node( elc_generator , node_id ) )
       continue;

      if( const auto secondary_s_r =
          block->get_secondary_spinning_reserve( generator ) ) {

       const auto secondary_spinning_reserve = & secondary_s_r[ t ];
       const auto dual = secondary_demand_constraints[ t ][ zone_id ].get_dual();
       linearization += - dual * secondary_spinning_reserve->get_value();
      }

     } // end( for each generator )
    } // end( for each node )
   } // end( for each zone )
  } // end( for each time instant )

 } // end( non-empty secondary demand constraints )

 // Add the contribution associated with the inertia demand constraints.

 const auto & inertia_demand_constraints =
  ucblock->get_inertia_demand_constraints();

 if( ! inertia_demand_constraints.empty() ) {

  const auto number_inertia_zones = ucblock->get_number_inertia_zones();

  for( Index t = 0 ; t < time_horizon ; ++t ) {
   for( Index zone_id = 0 ; zone_id < number_inertia_zones ; ++zone_id ) {
    for( Index node_id = 0 ; node_id < number_nodes ; ++node_id ) {
     if( ! ucblock->node_belongs_to_inertia_zone( node_id , zone_id ) )
      continue;

     // Compute the index of the first electrical generator of the current
     // UnitBlock.
     Index elc_generator = 0;
     for( Index unit_id = 0 ; unit_id < v_block_indices[ i ] ; ++unit_id ) {
      const auto unit_block = ucblock->get_unit_block( unit_id );
      elc_generator += unit_block->get_number_generators();
     }

     const auto num_generators = block->get_number_generators();

     for( Index generator = 0 ; generator < num_generators ;
          ++generator , ++elc_generator ) {

      if( ! ucblock->generator_belongs_to_node( elc_generator , node_id ) )
       continue;

      const auto dual = inertia_demand_constraints[ t ][ zone_id ].get_dual();

      // Commitment variable

      auto commitment = block->get_commitment( generator );
      auto inertia_commitment = block->get_inertia_commitment( generator );

      if( commitment && inertia_commitment ) {
       const auto commitment_t = & commitment[ t ];
       linearization +=
        - dual * inertia_commitment[ t ] * commitment_t->get_value();
      }

      // Active power variable

      auto active_power = block->get_active_power( generator );
      auto inertia_power = block->get_inertia_power( generator );

      if( active_power && inertia_power ) {

       auto active_power_t = & active_power[ t ];
       linearization +=
        - dual * inertia_power[ t ] * active_power_t->get_value();
      }

     } // end( for each generator )
    } // end( for each node )
   } // end( for each zone )
  } // end( for each time instant )

 } // end( non-empty inertia demand constraints )

 /* Finally, add the contribution associated with the objective function of
  * the UnitBlock.
  *
  * The objective function of a UnitBlock may have the form k*f(x), where k is
  * the scale factor. The contribution associated with the objective to the
  * linearization is therefore f(x). If k is non-zero, f(x) can be retrieved
  * by simply computing the objective and then dividing its value by k. If k
  * is zero, then we can temporarily scale the UnitBlock to 1, evaluate the
  * objective (whose value must then be f(x)), and finally scale the UnitBlock
  * back to its original scale factor. */

 auto objective =
  static_cast< FRealObjective * >( block->get_objective() );

 const auto scale = block->get_scale();

 if( scale != 0 ) {
  objective->compute();
  linearization += objective->value() / scale;
 }
 else {
  /* Scale the UnitBlock to 1 so that we can retrieve the value of the
   * objective associated with a single representative unit. No Modification
   * should be issued since the UnitBlock will be scaled back to the original
   * scale factor after the objective is computed. */
  block->scale( 1.0 , eNoMod , eNoMod );

  // Compute the Objective and retrieve its value.
  objective->compute();
  linearization += objective->value();

  // Scale the UnitBlock to its original scale factor.
  block->scale( scale , eNoMod , eNoMod );

  // Recompute the objective to take into account its original scale factor.
  objective->compute();
 }

 return linearization;
} // end( InvestmentFunction::compute_scale_linearization )

/*--------------------------------------------------------------------------*/

double InvestmentFunction::compute_kappa_linearization
( const IntermittentUnitBlock * intermittent_unit ) {

 /* The kappa constant associated with an IntermittentUnitBlock appears in the
  * following constraints for each time instant t:
  *
  * - The lower and upper bound constraints on the active power:
  *
  *   kappa * P^{mn}_{t} <= p^{ac}_{t} <= kappa * P^{mx}_{t}
  *
  * - The minimum total amount of power produced by the unit:
  *
  *   ( - p^{ac}_{t} + p^{pr}_{t} + p^{sc}_{t} ) <= - kappa * P^{mn}_{t}
  *
  * - The maximum total amount of power produced by the unit:
  *
  *   gamma * p^{ac}_{t} + p^{pr}_{t} + p^{sc}_{t} <= gamma * kappa * P^{mx}_{t}
  *
  * By letting lambda_min and lambda_max be the dual variables associated with
  * the lower and upper bound constraints on the active power, respectively,
  * and alpha_min and alpha_max be the dual variables associated with the
  * minimum and maximum total amount of power produced by the unit,
  * respectively, the linearization coefficient for the variable associated
  * with the IntermittentUnitBlock is
  *
  *   P^{mn} ' (lambda_min + alpha_min) -
  *   P^{mx} ' (lambda_max + gamma * alpha_max).
  */

 double linearization = 0;

 const auto gamma = intermittent_unit->get_gamma();
 const auto & max_power = intermittent_unit->get_maximum_power();
 const auto & min_power = intermittent_unit->get_minimum_power();

 // Minimum and maximum total power constraints

 const auto & min_power_constraints =
  intermittent_unit->get_min_power_constraints();
 const auto & max_power_constraints =
  intermittent_unit->get_max_power_constraints();

 // Lower and upper bound constraints on the active power

 const auto & active_power_bound_constraints =
  intermittent_unit->get_active_power_bound_constraints();

 /* The dual value of the bound constraint on the active power is associated
  * with either the lower bound or the upper bound constraint. This will help
  * determine to which bound the dual is associated with. */
 const auto obj_sign =
  ( intermittent_unit->get_objective_sense() == Objective::eMin ) ? - 1 : 1;

 const auto time_horizon = intermittent_unit->get_time_horizon();

 for( Index t = 0 ; t < time_horizon ; ++t ) {

  // Bound constraints on the active power

  double lambda_min;
  double lambda_max;

  const auto dual_value = active_power_bound_constraints[ t ].get_dual();

  if( obj_sign * dual_value >= 0 ) {
   // The dual value is associated with the lower bound constraint
   lambda_min = dual_value;
   lambda_max = 0;
  }
  else {
   // The dual value is associated with the upper bound constraint
   lambda_min = 0;
   lambda_max = dual_value;
  }

  // Minimum and maximum total power constraints

  double alpha_min = 0;
  if( ! min_power_constraints.empty() )
   alpha_min = min_power_constraints[ t ].get_dual();

  double alpha_max = 0;
  if( ! max_power_constraints.empty() )
   alpha_max = max_power_constraints[ t ].get_dual();

  // Finally, update the linearization

  linearization +=
   min_power[ t ] * ( lambda_min + alpha_min ) -
   max_power[ t ] * ( lambda_max + gamma * alpha_max );
 }

 return linearization;
}

/*--------------------------------------------------------------------------*/

void InvestmentFunction::update_linearization_unit_blocks( Index stage ) {

 /* The UnitBlocks that are subject to investment can be divided into two
  * groups, depending on how the investment is represented.
  *
  * The first group is formed by the UnitBlocks whose scale factors represent
  * the investment. These are the ThermalUnitBlock and the
  * BatteryUnitBlock. For these UnitBlocks, the linearization is impacted by
  * their objective function (as they are scaled) and the linking constraints
  * in the UCBlock.
  *
  * The second group is formed by the UnitBlocks whose kappa constants
  * represent the investment. These are the IntermittentUnitBlocks. For these
  * UnitBlocks, the linearization is impacted only by the constraints in which
  * the kappa constants appear, which are the constraints defined by
  * themselves.
  */

 const auto ucblock = get_ucblock( stage );

 for( Index i = 0 ; i < v_block_indices.size() ; ++i ) {

  const auto var_index = i;
  const auto block = ucblock->get_unit_block( v_block_indices[ i ] );

  if( dynamic_cast< const ThermalUnitBlock * >( block ) ||
      dynamic_cast< const BatteryUnitBlock * >( block ) ) {
   v_linearization[ var_index ] += compute_scale_linearization( i , stage );
  }
  else if( auto intermittent_unit =
           dynamic_cast< const IntermittentUnitBlock * >( block ) ) {
   v_linearization[ var_index ] +=
    compute_kappa_linearization( intermittent_unit );
  }
  else {
   // Unrecognized Block
   auto error_message = "InvestmentFunction::update_linearization: "
    "unrecognized UnitBlock: " + block->classname();
   if( ! block->name().empty() )
    error_message += " with name '" + block->name() + "'";
   error_message += ".";
   throw( std::logic_error( error_message ) );
  }
 } // end( for each UnitBlock )
} // end( InvestmentFunction::update_linearization_unit_blocks )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::update_linearization_network_blocks( Index stage ) {
 // Update the linearization with respect to the lines

 if( v_line_indices.empty() )
  // There is no investment in lines, so there is nothing to be done.
  return;

 const auto ucblock = get_ucblock( stage );
 const auto time_horizon = ucblock->get_time_horizon();

 for( Index t = 0 ; t < time_horizon ; ++t ) {

  const auto network_block = ucblock->get_network_block( t );

  if( const auto dc_network =
      dynamic_cast< const DCNetworkBlock * >( network_block ) ) {

   for( Index i = 0 ; i < v_line_indices.size() ; ++i ) {

    const auto var_index = v_block_indices.size() + i;

    // TODO

    // Finally, update the linearization

    // v_linearization[ var_index ] +=


   } // end( for each line )
  }
  else {
   // Unrecognized NetworkBlock
   auto error_message = "InvestmentFunction::update_linearization: "
    "unrecognized NetworkBlock: " + network_block->classname() + ".";
   throw( std::logic_error( error_message ) );
  }
 } // end( for each time instant )

} // end( InvestmentFunction::update_linearization_network_blocks )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::update_linearization() {

 const auto sddp_block = static_cast< SDDPBlock * >( v_Block.front() );
 const auto num_stages = sddp_block->get_time_horizon();

 auto retrieve_var_solution = [ this ]( Index stage ) {
  auto ucblock_solver = get_ucblock_solver( stage );
  if( ucblock_solver->has_var_solution() )
   ucblock_solver->get_var_solution();
  else
   throw( std::logic_error( "InvestmentFunction::update_linearization: primal "
                            "solution not available." ) );
 };

 auto retrieve_dual_solution = [ this ]( Index stage ) {
  auto ucblock_solver = get_ucblock_solver( stage );
  if( ucblock_solver->has_dual_solution() )
   ucblock_solver->get_dual_solution();
  else
   throw( std::logic_error( "InvestmentFunction::update_linearization: dual "
                            "solution not available." ) );
 };

 for( Index stage = 0 ; stage < num_stages ; ++stage ) {

  retrieve_dual_solution( stage );
  if( ! v_block_indices.empty() )
   retrieve_var_solution( stage );

  update_linearization_unit_blocks( stage );
  update_linearization_network_blocks( stage );
 } // end( for each stage )

}  // end( InvestmentFunction::update_linearization() )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::get_linearization_coefficients
( FunctionValue * g , Range range , Index name ) {

 range.second = std::min( range.second , Index( v_x.size() ) );
 if( range.second <= range.first )
  return;

 for( Index i = range.first ; i < range.second ; ++i ) {
  g[ i - range.first ] = v_linearization[ i ];
 }
}  // end( InvestmentFunction::get_linearization_coefficients( * , range ) )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::get_linearization_coefficients
( SparseVector & g , Range range , Index name ) {

 range.second = std::min( range.second , Index( v_x.size() ) );
 if( range.second <= range.first )
  return;

 for( Index i = range.first ; i < range.second ; ++i ) {
  g.coeffRef( i ) = v_linearization[ i ];
 }
}  // end( InvestmentFunction::get_linearization_coefficients( sv , range ) )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::get_linearization_coefficients
( FunctionValue * g , c_Subset & subset , const bool ordered , Index name ) {

 Index k = 0;
 for( auto i : subset ) {
  g[ k++ ] = v_linearization[ i ];
 }
}  // end( InvestmentFunction::get_linearization_coefficients( * , subset ) )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::get_linearization_coefficients
( SparseVector & g , c_Subset & subset , const bool ordered , Index name ) {

 for( auto i : subset ) {
  g.coeffRef( i ) = v_linearization[ i ];
 }
}  // end( InvestmentFunction::get_linearization_coefficients( sv, subset ) )

/*--------------------------------------------------------------------------*/

Function::FunctionValue
InvestmentFunction::get_linearization_constant( Index name ) {

 // TODO

 if( name == Inf<Index>() ) {
  // Linearization just computed and not in the global pool yet.

  if( f_diagonal_linearization_required ) {
   auto alpha = f_value;
   for( Index i = 0 ; i < v_linear_coefficients.size() ; ++i )
    alpha -= v_linear_coefficients[ i ] * v_x[ i ]->get_value();
   return alpha;
  }
  else {
   // "vertical" linearization
   // TODO
   //solver->get_dual_direction();
   return 0;
  }
 }
 else {
 }

 return 0;
}  // end( InvestmentFunction::get_linearization_constant )

/*--------------------------------------------------------------------------*/
/*-------------------------- PRIVATE METHODS -------------------------------*/
/*--------------------------------------------------------------------------*/

void InvestmentFunction::update_blocks() {

 const auto saved_f_ignore_modifications = f_ignore_modifications;

 f_ignore_modifications = true;

 const auto sddp_block = static_cast< SDDPBlock * >( v_Block.front() );
 const auto num_stages = sddp_block->get_time_horizon();

 for( Index stage = 0 ; stage < num_stages ; ++stage ) {

  auto ucblock = get_ucblock( stage );
  const auto time_horizon = ucblock->get_time_horizon();

  // Update the UnitBlocks

  for( Index i = 0 ; i < v_block_indices.size() ; ++i ) {

   const auto var_index = i;

   auto block = ucblock->get_unit_block( v_block_indices[ i ] );

   if( dynamic_cast< const ThermalUnitBlock * >( block ) ||
       dynamic_cast< const BatteryUnitBlock * >( block ) ) {
    block->scale( v_x[ var_index ]->get_value() );
   }
   else if( auto intermittent_unit =
            dynamic_cast< IntermittentUnitBlock * >( block ) ) {
    std::vector< double > kappa_vector = { v_x[ var_index ]->get_value() };
    intermittent_unit->set_kappa( kappa_vector.cbegin() );
   }
   else {
    // Unrecognized UnitBlock
    auto error_message = "InvestmentFunction::update_blocks: "
     "unrecognized UnitBlock: " + block->classname();
    if( ! block->name().empty() )
     error_message += " with name '" + block->name() + "'";
    error_message += ".";
    throw( std::logic_error( error_message ) );
   }
  } // end( for each UnitBlock )

  // Update the NetworkBlocks

  if( ! v_line_indices.empty() ) {

   // Collect the values of the kappa constants

   std::vector< double > kappa( v_line_indices.size() );
   for( Index i = 0 ; i < v_line_indices.size() ; ++i ) {
    const auto var_index = v_block_indices.size() + i;
    kappa[ i ] = v_x[ var_index ]->get_value();
   }

   // Now update the NetworkBlock for each time instant

   for( Index t = 0 ; t < time_horizon ; ++t ) {

    auto network_block = ucblock->get_network_block( t );

    if( auto dc_network =
        dynamic_cast< DCNetworkBlock * >( network_block ) ) {
     auto subset = v_line_indices;
     dc_network->set_kappa( kappa.cbegin() , std::move( subset ) );
    }
    else {
     // Unrecognized NetworkBlock
     auto error_message = "InvestmentFunction::update_blocks: "
      "unrecognized NetworkBlock: " + network_block->classname() + ".";
     throw( std::logic_error( error_message ) );
    }
   } // end( for each time instant )
  } // end( non-empty line indices )

 } // end( for each stage )

 f_ignore_modifications = saved_f_ignore_modifications;
 f_blocks_are_updated = true;
}  // end( InvestmentFunction::update_blocks )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::send_nuclear_modification
( const Observer::ChnlName chnl ) {
 // "nuclear modification" for Function: everything changed
 f_blocks_are_updated = false;
 if( f_Observer )
  f_Observer->add_Modification
   ( std::make_shared<FunctionMod>( this , FunctionMod::NaNshift ) , chnl );
}  // end( InvestmentFunction::send_nuclear_modification )

/*--------------------------------------------------------------------------*/
/*-------------------- End File InvestmentFunction.cpp ---------------------*/
/*--------------------------------------------------------------------------*/
