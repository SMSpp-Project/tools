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

const double dual_sign = -1.0; // TODO The Solver must provide the duals with
                               // the right sign

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

void InvestmentFunction::load( std::istream &input , char frmt ) {
 throw( std::logic_error( "InvestmentFunction::load(): "
                          "not implemented yet." ) );
}

/*--------------------------------------------------------------------------*/
/*------------ CONSTRUCTING AND DESTRUCTING InvestmentFunction -------------*/
/*--------------------------------------------------------------------------*/

InvestmentFunction::InvestmentFunction
( Block * inner_block , VarVector && x , IndexVector && asset_indices ,
  AssetTypeVector && asset_type , RealVector && linear_coefficients ,
  Observer * const observer )
 : C05Function( observer ) , f_blocks_are_updated( false ) ,
   f_solver_status( kUnEval ) , f_diagonal_linearization_required( false ) ,
   f_id( this ) {

 set_inner_block( inner_block );
 set_variables( std::move( x ) );

 v_asset_indices = std::move( asset_indices );
 v_asset_type = std::move( asset_type );
 v_linear_coefficients  = std::move( linear_coefficients );

 const auto num_assets = v_asset_indices.size();

 // default parameter values
 AAccMlt = get_dflt_dbl_par( dblAAccMlt );
 set_par( intGPMaxSz , C05Function::get_dflt_int_par( intGPMaxSz ) );
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

 Index num_assets;

 if( ! ::deserialize_dim( group , "NumAssets" , num_assets ) )
  num_assets = 0;

 if( ! v_x.empty() ) {
  if( num_assets != v_x.size() )
   throw std::logic_error( "InvestmentFunction::deserialize: the number of "
                           "assets to invest (" + std::to_string( num_assets ) +
                           ") is different from the number of active variables "
                           "(" + std::to_string( v_x.size() ) + ")." );
 }

 if( num_assets ) {

  // Deserialize the asset indices.

  ::deserialize( group , "Assets" , num_assets , v_asset_indices , false );

  // Deserialize the types of assets.

  if( ! ::deserialize( group , "AssetType" , num_assets , v_asset_type ,
                       true , true ) )
   v_asset_type.resize( num_assets , eUnitBlock );

  if( ! v_asset_type.empty() ) {
   if( v_asset_type.size() == 1 )
    v_asset_type.resize( num_assets , v_asset_type.front() );
   else if( v_asset_type.size() != num_assets )
    throw( std::logic_error( "InvestmentFunction::deserialize: the 'AssetType'"
                             " netCDF variable, if provided, must have size 0,"
                             " 1, or 'NumAssets'." ) );
  }

  // Deserialize the lower bound on the active variables

  ::deserialize( group , "LowerBound" , { num_assets } , v_lower_bound ,
                 true , true );

  if( ! v_lower_bound.empty() ) {
   if( v_lower_bound.size() == 1 )
    v_lower_bound.resize( num_assets , v_lower_bound.front() );
   else if( v_lower_bound.size() != num_assets )
    throw( std::logic_error( "InvestmentFunction::deserialize: the 'LowerBound'"
                             " netCDF variable, if provided, must have size 0,"
                             " 1, or 'NumAssets'." ) );
  }

  // Deserialize the linear coeffients of the objective function

  if( ::deserialize( group , "Cost" , num_assets ,
                     v_linear_coefficients , true , true ) ) {
   if( v_linear_coefficients.size() == 1 )
    v_linear_coefficients.resize( num_assets , v_linear_coefficients.front() );
   else if( v_linear_coefficients.size() != num_assets )
    throw( std::logic_error( "InvestmentFunction::deserialize: the 'Cost'"
                             " netCDF variable, if provided, must have size "
                             "0, 1, or 'NumAssets'." ) );
  }
  else {
   // All coefficients are zero.
   v_linear_coefficients.resize( num_assets , 0 );
  }

  // Deserialize the amount of assets currently installed in the system

  if( ::deserialize( group , "AmountInstalled" , num_assets ,
                     v_amount_installed , true , true ) ) {
   if( v_amount_installed.size() == 1 )
    v_amount_installed.resize( num_assets , v_amount_installed.front() );
   else if( v_amount_installed.size() != num_assets )
    throw( std::logic_error( "InvestmentFunction::deserialize: the "
                             "'InstalledCapacity' netCDF variable, if provided,"
                             " must have size 0, 1, or 'NumAssets'." ) );
  }

 } // end( if( num_assets ) )

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

void InvestmentFunction::set_default_inner_Block_BlockConfig() {
 if( auto inner_block = get_inner_block() ) {
  auto config = new OCRBlockConfig( inner_block );
  config->clear();
  config->apply( inner_block );
  delete config;
 }
}

/*--------------------------------------------------------------------------*/

void InvestmentFunction::set_default_inner_Block_BlockSolverConfig() {
 if( auto inner_block = get_inner_block() ) {
  auto solver_config = new RBlockSolverConfig( inner_block );
  solver_config->clear();
  solver_config->apply( inner_block );
  delete solver_config;
 }
}

/*--------------------------------------------------------------------------*/

void InvestmentFunction::set_ComputeConfig( ComputeConfig * scfg ) {

 auto inner_block = get_inner_block();
 if( ! inner_block )
  throw( std::logic_error( "InvestmentFunction::set_ComputeConfig: the inner "
                           "Block is not present." ) );

 if( ! scfg ) {
  // scfg is nullptr
  ThinComputeInterface::set_ComputeConfig();
  set_default_inner_Block_configuration();
  return;
 }

 if( ! scfg->f_extra_Configuration ) {
  // scfg->f_extra_Configuration is nullptr
  ThinComputeInterface::set_ComputeConfig( scfg );
  if( ! scfg->f_diff )
   set_default_inner_Block_configuration();
  return;
 }

 auto config_map = dynamic_cast
  < SimpleConfiguration< std::map< std::string , Configuration * > > * >
  ( scfg->f_extra_Configuration );

 if( ! config_map )
  // An invalid extra Configuration has not been provided.
  throw( std::invalid_argument( "InvestmentFunction::set_ComputeConfig: "
                                "invalid extra_Configuration." ) );

 ThinComputeInterface::set_ComputeConfig( scfg );

 for( const auto & [ key , config ] : config_map->f_value ) {

  if( key == "BlockConfig" ) {
   if( ! config ) {
    if( ! scfg->f_diff )
     // A BlockConfig for the inner Block was not provided. The inner Block is
     // configured to its default configuration.
     set_default_inner_Block_BlockConfig();
   }
   else if( auto block_config = dynamic_cast< BlockConfig * >( config ) )
    // A BlockConfig for the inner Block has been provided. Apply it.
    block_config->apply( inner_block );
   else
    // An invalid Configuration has been provided.
    throw( std::invalid_argument
           ( "InvestmentFunction::set_ComputeConfig: the Configuration "
             "associated with key \"BlockConfig\" is not a BlockConfig." ) );
  }
  else if( key == "BlockSolverConfig" ) {
   if( ! config ) {
    if( ! scfg->f_diff )
     // A BlockSolverConfig for the inner Block was not provided. The Solver
     // of the inner Block (and their sub-Block, recursively) are unregistered
     // and deleted.
     set_default_inner_Block_BlockSolverConfig();
   }
   else if( auto bsc = dynamic_cast< BlockSolverConfig * >( config ) )
    // A BlockSolverConfig for the inner Block has been provided. Apply it.
    bsc->apply( inner_block );
   else
    // An invalid Configuration has been provided.
    throw( std::invalid_argument
           ( "InvestmentFunction::set_ComputeConfig: the Configuration "
             "associated with key \"BlockSolverConfig\" is not a "
             "BlockSolverConfig." ) );
  }
  else {
   // An invalid key has been provided.
   throw( std::invalid_argument( "InvestmentFunction::set_ComputeConfig: "
                                 "invalid key: " + key ) );
  }
 }
}

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

void InvestmentFunction::set_par( const idx_type par , const int value ) {
 switch( par ) {
  case( intGPMaxSz ): {
   if( value < 0 )
    throw( std::invalid_argument( "InvestmentFunction::set_par: intGPMaxSz "
                                  "must be non-negative" ) );

   auto old_size = global_pool.size();

   global_pool.resize( value );

   if( f_Observer && ( decltype( old_size )( value ) < old_size ) ) {
    // The size of the global pool is being reduced. We store in "which" the
    // indices of the deleted linearizations.
    Subset which( global_pool.size() - value );
    std::iota( which.begin() , which.end() , value );
    f_Observer->add_Modification
     ( std::make_shared<C05FunctionMod>
       ( this , C05FunctionMod::GlobalPoolRemoved , std::move( which ) , 0 ) );
   }

   break;
  }

  default: C05Function::set_par( par , value );
 }
}  // end( InvestmentFunction::set_par )

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

 // Erase the asset index, asset type, and the linear coefficient associated
 // with the Variable being removed
 v_asset_indices.erase( v_asset_indices.begin() + i );
 v_asset_type.erase( v_asset_type.begin() + i );

 f_blocks_are_updated = false;
 generator_node_map.clear(); // the generator map must be rebuilt

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
 generator_node_map.clear(); // the generator map must be rebuilt

 if( ( range.first == 0 ) && ( range.second == Index( v_x.size() ) ) ) {
  // removing *all* Variables
  Vec_p_Var vars( v_x.size() );

  for( decltype( v_x )::size_type i = 0 ; i < v_x.size() ; ++i )
   vars[ i ] = v_x[ i ];

  v_x.clear();
  v_asset_indices.clear();
  v_asset_type.clear();
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

  // Iterators to the elements to be removed

  const auto v_asset_indices_it =
   std::make_pair( v_asset_indices.begin() + range.first ,
                   v_asset_indices.begin() + range.second );

  const auto v_asset_type_it =
   std::make_pair( v_asset_type.begin() + range.first ,
                   v_asset_type.begin() + range.second );

  const auto v_linear_coefficients_it =
   std::make_pair( v_linear_coefficients.begin() + range.first ,
                   v_linear_coefficients.begin() + range.second );

  v_x.erase( v_x_it.first , v_x_it.second );
  v_asset_indices.erase( v_asset_indices_it.first , v_asset_indices_it.second );
  v_asset_type.erase( v_asset_type_it.first , v_asset_type_it.second );
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
  v_asset_indices.clear();
  v_asset_type.clear();
  v_linear_coefficients.clear();

  f_blocks_are_updated = false;
  generator_node_map.clear(); // the generator map must be rebuilt

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
 generator_node_map.clear(); // the generator map must be rebuilt

 const auto erase = [ this , &indices ]() {
  compact( v_asset_indices , indices );
  compact( v_asset_type , indices );
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

 auto NumAssets = group.addDim( "NumAssets" , v_asset_indices.size() );

 ::serialize( group , "Assets" , netCDF::NcUint() , NumAssets ,
              v_asset_indices );

 ::serialize( group , "AssetType" , netCDF::NcUbyte() , NumAssets ,
              v_asset_type );

 ::serialize( group , "LowerBound" , netCDF::NcDouble() , NumAssets ,
              v_lower_bound );

 ::serialize( group , "Cost" , netCDF::NcDouble() , NumAssets ,
              v_linear_coefficients );

 ::serialize( group , "AmountInstalled" , netCDF::NcDouble() , NumAssets ,
              v_amount_installed );

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

 // For the InvestmentFunction to be correctly computed, the inner Block
 // cannot be modified by other entities. Therefore, the inner Block must be
 // locked.

 // Try to lock the inner Block.
 auto owned = v_Block.front()->is_owned_by( f_id );
 if( ( ! owned ) && ( ! v_Block.front()->lock( f_id ) ) )
  return( kError ); // If this does not work, this is clearly an error.

 // Since the inner Solver may need to lock the inner Block, the
 // InvestmentFunction lends its identity to the inner Solver.

 solver->set_id( f_id );

 if( generator_node_map.empty() )
  build_generator_node_map();

 if( changedvars || ( ! f_blocks_are_updated ) ) {
  // Update the Blocks.

  try {
   update_blocks();
  }
  catch( const std::exception & e ) {
   // An error occurred whule updating the Blocks.
   if( ! owned )
    v_Block.front()->unlock( f_id );  // unlock the inner Block
   std::cout << "InvestmentFunction::compute(): an error occurred while "
    "updating the Blocks: '" << e.what() << "'" << std::endl;
   return( kError );
  }
 }

 const auto sddp_block = static_cast< SDDPBlock * >( v_Block.front() );
 const auto num_scenarios = sddp_block->get_scenario_set().size();
 f_value = 0.0;
 reset_linearization();

 for( int scenario = 0 ; scenario < num_scenarios ; ++scenario ) {
  solver->set_par( SDDPGreedySolver::intScenarioId , scenario );

  const auto saved_f_ignore_modifications = f_ignore_modifications;
  f_ignore_modifications = true;

  f_solver_status = solver->compute( true );

  f_ignore_modifications = saved_f_ignore_modifications;

  if( ! solver->has_var_solution() )
   return( f_solver_status );

  f_value += solver->get_var_value();

  try {
   update_linearization();
  }
  catch( const std::exception & e ) {
   // An error occurred while updating the linearization.
   if( ! owned )
    v_Block.front()->unlock( f_id );  // unlock the inner Block
   std::cout << "InvestmentFunction::compute(): an error occurred while "
    "updating the linearization: '" << e.what() << "'" << std::endl;
   return( kError );
  }
 }

 // Compute the expectation of the operational costs

 f_value /= num_scenarios;

 // Compute the expectation of the linearization

 for( Index i = 0 ; i < v_linearization.size() ; ++i ) {
  v_linearization[ i ] /= num_scenarios;
 }

 f_linearization_constant /= num_scenarios;

 // Update the linearization constant to take into account the objective value

 f_linearization_constant += f_value;

 // Consider the linear term of the objective

 for( Index i = 0 ; i < v_linear_coefficients.size() ; ++i ) {

  const auto amount_installed = get_amount_installed( i );

  // Update the objective value
  f_value += v_linear_coefficients[ i ] * ( get_var_value( i , false ) -
                                            amount_installed );

  // Update the linearization
  v_linearization[ i ] += v_linear_coefficients[ i ];

  // Update the linearization constant
  if( f_reformulated_bounds && ( i < v_lower_bound.size() ) &&
      ( v_lower_bound[ i ] > -Inf< double >() ) ) {
   f_linearization_constant += v_linear_coefficients[ i ] * v_lower_bound[ i ];
  }

  f_linearization_constant -= v_linear_coefficients[ i ] * amount_installed;
 }

 // Unlock the inner Block if it is necessary
 if( ! owned )
  v_Block.front()->unlock( f_id );

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
 return( get_inner_block_objective_sense() == Objective::eMin );
}

/*--------------------------------------------------------------------------*/

bool InvestmentFunction::is_concave( void ) const {
 if( v_Block.empty() )
  return false;
 return( get_inner_block_objective_sense() == Objective::eMax );
}

/*--------------------------------------------------------------------------*/

bool InvestmentFunction::has_linearization( const bool diagonal ) {

 auto solver = get_solver< CDASolver >();

 if( ! solver )
  return false;

 if( diagonal ) {
  f_diagonal_linearization_required = true;
  return solver->has_var_solution() && solver->has_dual_solution();
 }
 else {
  f_diagonal_linearization_required = false;
  // TODO
  throw( std::logic_error( "InvestmentFunction::has_linearization: vertical "
                           "linearization not implemented yet." ) );
 }
}  // end( InvestmentFunction::has_linearization )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::store_linearization( Index name , ModParam issueMod ) {
 if( name >= global_pool.size() )
  throw( std::invalid_argument( "InvestmentFunction::store_linearization: "
                                "invalid global pool name: " +
                                std::to_string( name ) ) );

 global_pool.store( get_linearization_constant() , v_linearization , name ,
                    f_diagonal_linearization_required );

 if( ( ! f_Observer ) || ( ! f_Observer->issue_mod( issueMod ) ) )
  return;

 f_Observer->add_Modification( std::make_shared<C05FunctionMod>
                               ( this , C05FunctionMod::GlobalPoolAdded ,
                                 Subset( { name } ) , 0 ,
                                 Observer::par2concern( issueMod ) ) ,
                               Observer::par2chnl( issueMod ) );

} // end InvestmentFunction::store_linearization( Index )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::store_combination_of_linearizations
( c_LinearCombination & coefficients , Index name , ModParam issueMod ) {

 global_pool.store_combination_of_linearizations( coefficients , name ,
                                                  AAccMlt );

 if( ( ! f_Observer ) || ( ! f_Observer->issue_mod( issueMod ) ) )
  return;

 f_Observer->add_Modification( std::make_shared<C05FunctionMod>
                               ( this , C05FunctionMod::GlobalPoolAdded ,
                                 Subset( { name } ) , 0 ,
                                 Observer::par2concern( issueMod ) ) ,
                               Observer::par2chnl( issueMod ) );

}  // end( InvestmentFunction::store_combination_of_linearizations )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::delete_linearization( const Index name ,
                                               ModParam issueMod ) {
 global_pool.delete_linearization( name );

 if( ( ! f_Observer ) || ( ! f_Observer->issue_mod( issueMod ) ) )
  return;

 f_Observer->add_Modification( std::make_shared<C05FunctionMod>
                               ( this , C05FunctionMod::GlobalPoolRemoved ,
                                 Subset( { name } ) , 0 ,
                                 Observer::par2concern( issueMod ) ) ,
                               Observer::par2chnl( issueMod ) );
}  // end( InvestmentFunction::delete_linearization )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::delete_linearizations( Subset && which , bool ordered ,
                                                ModParam issueMod ) {
 global_pool.delete_linearizations( which , ordered );

 if( ( ! f_Observer ) || ( ! f_Observer->issue_mod( issueMod ) ) )
  return;

 f_Observer->add_Modification( std::make_shared<C05FunctionMod>
                               ( this , C05FunctionMod::GlobalPoolRemoved ,
                                 std::move( which ) , 0 ,
                                 Observer::par2concern( issueMod ) ) ,
                               Observer::par2chnl( issueMod ) );
}

/*--------------------------------------------------------------------------*/

void InvestmentFunction::get_linearization_coefficients
( FunctionValue * g , Range range , Index name ) {

 if( name != Inf< Index >() )
  throw( std::logic_error( "InvestmentFunction::get_linearization_coefficients: "
                           "linearization from global pool not implemented yet." ) );

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

 if( name != Inf< Index >() )
  throw( std::logic_error( "InvestmentFunction::get_linearization_coefficients: "
                           "linearization from global pool not implemented yet." ) );

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

 if( name != Inf< Index >() )
  throw( std::logic_error( "InvestmentFunction::get_linearization_coefficients: "
                           "linearization from global pool not implemented yet." ) );

 Index k = 0;
 for( auto i : subset ) {
  g[ k++ ] = v_linearization[ i ];
 }
}  // end( InvestmentFunction::get_linearization_coefficients( * , subset ) )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::get_linearization_coefficients
( SparseVector & g , c_Subset & subset , const bool ordered , Index name ) {

 if( name != Inf< Index >() )
  throw( std::logic_error( "InvestmentFunction::get_linearization_coefficients: "
                           "linearization from global pool not implemented yet." ) );

 for( auto i : subset ) {
  g.coeffRef( i ) = v_linearization[ i ];
 }
}  // end( InvestmentFunction::get_linearization_coefficients( sv, subset ) )

/*--------------------------------------------------------------------------*/

Function::FunctionValue
InvestmentFunction::get_linearization_constant( Index name ) {

 if( name == Inf<Index>() ) {
  // Linearization just computed and not in the global pool yet.

  if( f_diagonal_linearization_required ) {
   auto alpha = f_value;
   for( Index i = 0 ; i < v_linearization.size() ; ++i ) {
    alpha -= v_linearization[ i ] * get_var_value( i );
   }

   return alpha;
  }
  else {
   // TODO
   throw( std::logic_error( "InvestmentFunction::get_linearization_constant: "
                            "vertical linearization not implemented yet." ) );
  }
 }
 else {
  // TODO
  throw( std::logic_error( "InvestmentFunction::get_linearization_constant: "
                           "linearization from global pool not implemented "
                           "yet." ) );
 }

 return 0;
}  // end( InvestmentFunction::get_linearization_constant )

/*--------------------------------------------------------------------------*/

Function::FunctionValue InvestmentFunction::get_value( void ) const {
 auto solver = get_solver();
 if( solver->has_var_solution() )
  return f_value;
 if( get_inner_block_objective_sense() == Objective::eMin )
  return Inf< double >();
 return -Inf< double >();
} // end ( InvestmentFunction::get_value )

/*--------------------------------------------------------------------------*/
/*-------------------- Methods for handling Modification -------------------*/
/*--------------------------------------------------------------------------*/

void InvestmentFunction::add_Modification( sp_Mod mod ,
                                           Observer::ChnlName chnl ) {
 if( f_ignore_modifications )
  return;
 send_nuclear_modification( chnl );

}  // end( InvestmentFunction::add_Modification )

/*--------------------------------------------------------------------------*/
/*---------------- PRIVATE METHODS OF THE InvestmentFunction ---------------*/
/*--------------------------------------------------------------------------*/

int InvestmentFunction::get_inner_block_objective_sense() const {
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
 v_linearization.assign( v_x.size() , 0 );
 f_linearization_constant = 0;
}

/*--------------------------------------------------------------------------*/

Index InvestmentFunction::get_node( Index stage , Index block_index ,
                                    Index generator ) const {
 // i is between 0 and the number of UnitBlock assets - 1.
 const auto i = v_block_indices_map[ block_index ];
 return generator_node_map[ stage ][ i ][ generator ];
}

/*--------------------------------------------------------------------------*/

void InvestmentFunction::build_generator_node_map() {

 // The indices of the UnitBlocks
 std::vector< Index > block_indices;
 block_indices.reserve( v_asset_indices.size() );

 for( Index i = 0 ; i < v_asset_indices.size() ; ++i ) {
  if( v_asset_type[ i ] == eUnitBlock )
   block_indices.push_back( v_asset_indices[ i ] );
 }

 if( block_indices.empty() )
  return;

 v_block_indices_map.resize
  ( 1 + * std::max_element( block_indices.cbegin() , block_indices.cend() ) ,
    Inf< Index >() );
 for( Index i = 0 ; i < block_indices.size() ; ++i ) {
  v_block_indices_map[ block_indices[ i ] ] = i;
 }

 const auto sddp_block = static_cast< SDDPBlock * >( v_Block.front() );
 const auto num_stages = sddp_block->get_time_horizon();

 generator_node_map.resize( num_stages );

 for( Index stage = 0 ; stage < num_stages ; ++stage ) {

  generator_node_map[ stage ].resize( block_indices.size() );

  const auto ucblock = get_ucblock( stage );
  const auto network_data = ucblock->get_NetworkData();
  const auto number_nodes = network_data ? network_data->get_number_nodes() : 1;

  if( number_nodes <= 1 ) {
   // Since there is only one node, all generators belong to the same node
   // (node 0).
   for( Index i = 0 ; i < block_indices.size() ; ++i ) {
    const auto unit_block = ucblock->get_unit_block( block_indices[ i ] );
    const auto num_generators = unit_block->get_number_generators();
    generator_node_map[ stage ][ i ].resize( num_generators , 0 );
   }
   continue;
  }

  const auto number_units = ucblock->get_number_units();
  const auto & generator_node = ucblock->get_generator_node();

  for( Index node_id = 0 ; node_id < number_nodes ; ++node_id ) {

   Index elc_generator = 0;
   for( Index unit_id = 0 ; unit_id < number_units ; ++unit_id ) {

    const auto unit_block = ucblock->get_unit_block( unit_id );
    const auto num_generators = unit_block->get_number_generators();

    auto it = std::find( block_indices.cbegin() ,
                         block_indices.cend() , unit_id );

    const auto index = std::distance( block_indices.cbegin() , it );

    if( index == block_indices.size() ) {
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

double InvestmentFunction::compute_scale_linearization
( Index block_index , Index stage ) {

 /* TODO The following code does not take into account the pollutant budget
  * constraints and the heat constraints. When these constraints are correctly
  * implemented, this function must be updated. */

 const auto ucblock = get_ucblock( stage );
 const auto network_data = ucblock->get_NetworkData();
 const auto number_nodes = network_data ? network_data->get_number_nodes() : 1;
 const auto time_horizon = ucblock->get_time_horizon();

 const auto block = ucblock->get_unit_block( block_index );

 // This is the contribution to the linearization associated with this
 // UnitBlock.
 double linearization = 0;

 // Add the contribution associated with the node injection constraints

 const auto & node_injection_constraints =
  ucblock->get_node_injection_constraints();

 for( Index t = 0 ; t < time_horizon ; ++t ) {

  for( Index g = 0 ; g < block->get_number_generators() ; ++g ) {

   const auto node = get_node( stage , block_index , g );
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
     for( Index unit_id = 0 ; unit_id < block_index ; ++unit_id ) {
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
       const auto dual =
        std::abs( primary_demand_constraints[ t ][ zone_id ].get_dual() );
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
     for( Index unit_id = 0 ; unit_id < block_index ; ++unit_id ) {
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
       const auto dual =
        std::abs( secondary_demand_constraints[ t ][ zone_id ].get_dual() );
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
     for( Index unit_id = 0 ; unit_id < block_index ; ++unit_id ) {
      const auto unit_block = ucblock->get_unit_block( unit_id );
      elc_generator += unit_block->get_number_generators();
     }

     const auto num_generators = block->get_number_generators();

     for( Index generator = 0 ; generator < num_generators ;
          ++generator , ++elc_generator ) {

      if( ! ucblock->generator_belongs_to_node( elc_generator , node_id ) )
       continue;

      const auto dual =
       std::abs( inertia_demand_constraints[ t ][ zone_id ].get_dual() );

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

 /* Finally, add the contribution associated with the objective function (if
  * any) of the UnitBlock.
  *
  * The objective function of a UnitBlock may have the form k*f(x), where k is
  * the scale factor. The contribution associated with the objective to the
  * linearization is therefore f(x). If k is non-zero, f(x) can be retrieved
  * by simply computing the objective and then dividing its value by k. If k
  * is zero, then we can temporarily scale the UnitBlock to 1, evaluate the
  * objective (whose value must then be f(x)), and finally scale the UnitBlock
  * back to its original scale factor. */

 if( auto objective =
     dynamic_cast< FRealObjective * >( block->get_objective() ) ) {

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
 }

 return linearization;
} // end( InvestmentFunction::compute_scale_linearization )

/*--------------------------------------------------------------------------*/

double InvestmentFunction::compute_kappa_linearization
( IntermittentUnitBlock * intermittent_unit , Index var_index ) {

 /* The kappa constant associated with an IntermittentUnitBlock appears in the
  * following constraints for each time instant t:
  *
  * - The lower and upper bound constraints on the active power:
  *
  *   kappa * P^{mn}_{t} <= p^{ac}_{t} <= kappa * P^{mx}_{t}
  *
  * - The minimum total amount of power produced by the unit:
  *
  *   kappa * P^{mn}_{t} <= p^{ac}_{t} - p^{pr}_{t} - p^{sc}_{t}
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

 // Lower bound on the kappa variable

 const auto var_lower_bound = get_var_lower_bound( var_index );

 /* The dual value of the bound constraint on the active power is associated
  * with either the lower bound or the upper bound constraint. This will help
  * determine to which bound the dual is associated with. */
 const auto obj_sign =
  ( intermittent_unit->get_objective_sense() == Objective::eMin ) ? - 1 : 1;

 const auto time_horizon = intermittent_unit->get_time_horizon();

 for( Index t = 0 ; t < time_horizon ; ++t ) {

  // Bound constraints on the active power

  double lambda_min = 0;
  double lambda_max = 0;

  // Retrieve the dual associated with the bound constraints

  if( ! active_power_bound_constraints.empty() ) {
   const auto bound_dual =
    active_power_bound_constraints[ t ].get_dual() * dual_sign;

   // Now determine the dual value associated with each bound

   if( active_power_bound_constraints[ t ].get_lhs() ==
       active_power_bound_constraints[ t ].get_rhs() ) {
    // Equality constraint
    lambda_max = bound_dual;
   }
   else if( obj_sign * bound_dual >= 0 ) {
    // The dual value is associated with the lower bound constraint
    lambda_min = std::abs( bound_dual );
   }
   else {
    // The dual value is associated with the upper bound constraint
    lambda_max = std::abs( bound_dual );
   }
  } // end( ! active_power_bound_constraints.empty() )

  // Minimum and maximum total power constraints

  double alpha_min = 0;
  if( ! min_power_constraints.empty() )
   alpha_min = std::abs( min_power_constraints[ t ].get_dual() );

  double alpha_max = 0;
  if( ! max_power_constraints.empty() )
   alpha_max = std::abs( max_power_constraints[ t ].get_dual() );

  // Finally, update the linearization

  // Update the linearization coefficient

  linearization +=
   min_power[ t ] * ( lambda_min + alpha_min ) -
   max_power[ t ] * ( lambda_max + gamma * alpha_max );

  // Update the linearization constant

  if( const auto p_ac = intermittent_unit->get_active_power( 0 ) )
   f_linearization_constant += p_ac[ t ].get_value() *
    ( lambda_max - lambda_min + gamma * alpha_max - alpha_min );

  if( const auto p_pr = intermittent_unit->get_primary_spinning_reserve( 0 ) )
   f_linearization_constant += p_pr[ t ].get_value() *
    ( alpha_min + alpha_max );

  if( const auto p_sc = intermittent_unit->get_secondary_spinning_reserve( 0 ) )
   f_linearization_constant += p_sc[ t ].get_value() *
    ( alpha_min + alpha_max );

  // If the bounds on the variables have been reformulated, consider the
  // contribution of the lower bound.

  if( f_reformulated_bounds && ( var_lower_bound > -Inf< double >() ) ) {

   // Lower bound constraint
   f_linearization_constant +=   lambda_min * min_power[ t ] * var_lower_bound;

   // Upper bound constraint
   f_linearization_constant += - lambda_max * max_power[ t ] * var_lower_bound;

   // Minimum total power constraint
   f_linearization_constant += alpha_min * min_power[ t ] * var_lower_bound;

   // Maximum total power constraint
   f_linearization_constant +=
    - alpha_max * gamma * max_power[ t ] * var_lower_bound;
  }
 }

 return linearization;
}

/*--------------------------------------------------------------------------*/

double InvestmentFunction::compute_kappa_linearization
( const BatteryUnitBlock * unit , Index var_index ) {

 /* The kappa constant associated with a BatteryUnitBlock appears in the
  * following constraints for each time instant t:
  *
  * - Minimum and maximum power output constraint (lambda):
  *
  *   kappa * P^{min}_{t} <= p^{ac}_{t} - p^{pr}_{t} - p^{sc}_{t}  [lambda_min]
  *
  *   p^{ac}_{t} + p^{pr}_{t} + p^{sc}_{t} <= kappa * P^{max}_{t}  [lambda_max]
  *
  * - Intake and outtake level bounds (alpha):
  *
  *   p^{+}_{t} <= kappa * P^{max}_{t}                             [alpha_max]
  *
  *   p^{+}_{t} <= kappa * u^{+}_t * P^{max}_{t}                   [alpha_max_u]
  *
  *   p^{-}_{t} <= - kappa * (1 - u^{+}_t) * P^{min}_{t}           [alpha_min_u]
  *
  * - Storage level bounds (beta):
  *
  *   kappa * V^{min}_t <= v_t                                     [beta_min]
  *
  *   v_t <= kappa * V^{max}_t                                     [beta_max]
  *
  * - Primary and secondary reserves bounds (gamma):
  *
  *   p^{pr}_t <= kappa P^{pr max}_t                               [gamma_pr]
  *
  *   p^{sc}_t <= kappa P^{sc max}_t                               [gamma_sc]
  *
  * The name between [] represents the dual variable associated with each
  * constraint. The linearization coefficient for the investment variable
  * associated with the BatteryUnitBlock is
  *
  *   P^{min} ' (lambda_min + (1 - u^+) * alpha_min_u) -
  *   P^{max} ' (lambda_max + alpha_max + u^+ * alpha_max_u) +
  *   V^{min} ' beta_min - V^{max} ' beta_max -
  *   P^{pr max} ' gamma_pr - P^{sc max} ' gamma_sc
  */

 double linearization = 0;

 // Minimum and maximum power output constraint

 const auto & min_power_constraints = unit->get_min_power_constraints();

 const auto & max_power_constraints = unit->get_max_power_constraints();

 // Intake and outtake level bounds

 const auto & intake_bound_constraints = unit->get_max_intake_constraints();

 const auto & max_intake_binary_constraints =
  unit->get_max_intake_binary_constraints();

 const auto & max_outtake_binary_constraints =
  unit->get_max_outtake_binary_constraints();

 const auto & u = unit->get_intake_outtake_binary_variables();

 // Storage level bounds

 const auto & storage_level_bound_constraints =
  unit->get_storage_level_bound_constraints();

 // Primary and secondary reserves bounds

 const auto & primary_reserve_bounds = unit->get_primary_reserve_bounds();

 const auto & secondary_reserve_bounds = unit->get_secondary_reserve_bounds();

 /* The dual value of a constraint that has both finite lower and upper bounds
  * is associated with either the lower bound or the upper bound
  * constraint. This will help determine to which bound the dual is associated
  * with. */
 const auto obj_sign =
  ( unit->get_objective_sense() == Objective::eMin ) ? - 1 : 1;

 const auto time_horizon = unit->get_time_horizon();

 for( Index t = 0 ; t < time_horizon ; ++t ) {

  const auto min_power = unit->get_minimum_power( t );
  const auto max_power = unit->get_maximum_power( t );
  const auto min_storage = unit->get_minimum_storage( t );
  const auto max_storage = unit->get_maximum_storage( t );

  // Minimum and maximum power output constraint

  const auto lambda_min = std::abs( min_power_constraints[ t ].get_dual() );
  const auto lambda_max = std::abs( max_power_constraints[ t ].get_dual() );

  linearization += min_power * lambda_min - max_power * lambda_max;

  // Intake and outtake level bounds

  if( ! intake_bound_constraints.empty() ) {
   double alpha_max = 0;

   const auto dual = intake_bound_constraints[ t ].get_dual() * dual_sign;
   if( intake_bound_constraints[ t ].get_lhs() == 0.0 ) {
    // The constraint has a zero lower bound.
    if( obj_sign * dual < 0 )
     // The bound is associated with the upper bound constraint.
     alpha_max = std::abs( dual );
   }
   else {
    // The constraint must not have a lower bound
    assert( intake_bound_constraints[ t ].get_lhs() == - Inf< double >() );
    // and therefore the dual is associated with the upper bound constraint.
    alpha_max = std::abs( dual );
   }

   linearization += - alpha_max * max_power;
  }

  if( ! max_intake_binary_constraints.empty() ) {
   const auto alpha_max_u =
    std::abs( max_intake_binary_constraints[ t ].get_dual() );
   linearization += - alpha_max_u * u[ t ].get_value() * max_power;
  }

  if( ! max_outtake_binary_constraints.empty() ) {
   const auto alpha_min_u =
    std::abs( max_outtake_binary_constraints[ t ].get_dual() );
   linearization += ( 1.0 - u[ t ].get_value() ) * alpha_min_u * min_power;
  }

  // Storage level bounds

  const auto dual = storage_level_bound_constraints[ t ].get_dual() * dual_sign;
  if( obj_sign * dual >= 0 ) {
   // The bound is associated with the lower bound constraint.
   linearization += min_storage * std::abs( dual );
  }
  else {
   // The bound is associated with the upper bound constraint.
   linearization += - max_storage * std::abs( dual );
  }

  // Primary and secondary reserves bounds

  if( ! primary_reserve_bounds.empty() ) {
   const auto gamma_pr = std::abs( primary_reserve_bounds[ t ].get_dual() );
   linearization += - unit->get_maximum_primary_power( t ) * gamma_pr;
  }

  if( ! secondary_reserve_bounds.empty() ) {
   const auto gamma_sc = std::abs( secondary_reserve_bounds[ t ].get_dual() );
   linearization += - unit->get_maximum_secondary_power( t ) * gamma_sc;
  }

 }

 return linearization;
}

/*--------------------------------------------------------------------------*/

void InvestmentFunction::update_linearization_unit_blocks
( Index stage ,
  const std::vector< std::pair< Index , Index > > & block_indices ) {

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

 for( const auto & [ block_index , var_index ] : block_indices ) {

  auto block = ucblock->get_unit_block( block_index );

  if( dynamic_cast< const ThermalUnitBlock * >( block ) ) {
   v_linearization[ var_index ] += compute_scale_linearization( block_index ,
                                                                stage );
  }
  else if( auto unit = dynamic_cast< BatteryUnitBlock * >( block ) ) {
   if( f_scale_battery )
    v_linearization[ var_index ] += compute_scale_linearization( block_index ,
                                                                 stage );
   else
    v_linearization[ var_index ] += compute_kappa_linearization( unit ,
                                                                 var_index );
  }
  else if( auto unit = dynamic_cast< IntermittentUnitBlock * >( block ) ) {
   if( f_scale_intermittent )
    v_linearization[ var_index ] += compute_scale_linearization( block_index ,
                                                                 stage );
   else
    v_linearization[ var_index ] += compute_kappa_linearization( unit ,
                                                                 var_index );
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

void InvestmentFunction::update_linearization_network_blocks
( Index stage ,
  const std::vector< std::pair< Index , Index > > & line_indices ) {

 // Update the linearization with respect to the lines

 if( line_indices.empty() )
  // There is no investment in lines, so there is nothing to be done.
  return;

 const auto ucblock = get_ucblock( stage );
 const auto time_horizon = ucblock->get_time_horizon();

 for( Index t = 0 ; t < time_horizon ; ++t ) {

  const auto network_block = ucblock->get_network_block( t );

  if( const auto dc_network =
      dynamic_cast< const DCNetworkBlock * >( network_block ) ) {

   // HVDC lines

   const auto network_data = ucblock->get_NetworkData();
   assert( ( ! network_data ) ||
           network_data->get_lines_type() == NetworkBlock::kHVDC );

   const auto & constraints = dc_network->get_power_flow_limit_HVDC_bounds();

   if( constraints.empty() )
    continue;

   /* For each line l, the flow limit constraints on that line are:
    *
    *     kappa_l * Pmin_l <= power_flow_l <= kappa_l * Pmax_l
    *
    * where power_flow_l is the power flow on the line l and Pmin_l and Pmax_l
    * are the minimum and maximum power flow on the line l, respectively. */

   /* The dual value of the flow limit constraint on the power flow is
    * associated with either the lower bound or the upper bound
    * constraint. This will help determine to which bound the dual is
    * associated with. */
   const auto obj_sign =
    ( dc_network->get_objective_sense() == Objective::eMin ) ? - 1 : 1;

   for( const auto & [ line , var_index ] : line_indices ) {

    const auto dual = constraints[ line ].get_dual() * dual_sign;
    const auto min_flow = dc_network->get_min_power_flow( line );
    const auto max_flow = dc_network->get_max_power_flow( line );

    // Dual value associated with the lower bound constraint.
    double lambda_min;

    // Dual value associated with the upper bound constraint.
    double lambda_max;

    if( obj_sign * dual >= 0 ) {
     // The dual value is associated with the lower bound constraint.
     lambda_min = std::abs( dual );
     lambda_max = 0;
    }
    else {
     // The dual value is associated with the upper bound constraint.
     lambda_min = 0;
     lambda_max = std::abs( dual );
    }

    // Finally, update the linearization.

    // Update the linearization coefficient.

    v_linearization[ var_index ] +=
     lambda_min * min_flow - lambda_max * max_flow;

    // Update the linearization constant.

    const auto power_flow = dc_network->get_power_flow();
    if( ! power_flow.empty() ) {
     f_linearization_constant +=
      ( lambda_max - lambda_min ) * power_flow[ line ].get_value();
    }

    // If the bounds on the variables have been reformulated, consider the
    // contribution of the lower bound.

    const auto var_lower_bound = get_var_lower_bound( var_index );

    if( f_reformulated_bounds && ( var_lower_bound > -Inf< double >() ) ) {
     f_linearization_constant +=
      var_lower_bound * ( lambda_min * min_flow - lambda_max * max_flow );
    }

   } // end( for each line )
  } // end( dynamic_cast< const DCNetworkBlock * > )
  else {
   // Unrecognized NetworkBlock
   auto error_message = "InvestmentFunction::update_linearization_network_"
    "blocks: unrecognized NetworkBlock: " + network_block->classname() + ".";
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
   throw( std::logic_error( "InvestmentFunction::update_linearization: "
                            "primal solution not available." ) );
 };

 auto retrieve_dual_solution = [ this ]( Index stage ) {
  auto ucblock_solver = get_ucblock_solver( stage );
  if( ucblock_solver->has_dual_solution() )
   ucblock_solver->get_dual_solution();
  else
   throw( std::logic_error( "InvestmentFunction::update_linearization: "
                            "dual solution not available." ) );
 };

 // The indices of the UnitBlocks and the indices of their variables
 std::vector< std::pair< Index , Index > > block_indices;
 block_indices.reserve( v_asset_indices.size() );

 // The indices of the transmission lines and the indices of their variables
 std::vector< std::pair< Index , Index > > line_indices;
 line_indices.reserve( v_asset_indices.size() );

 for( Index i = 0 ; i < v_asset_indices.size() ; ++i ) {

  const auto asset_type =  v_asset_type[ i ];
  const auto asset_index =  v_asset_indices[ i ];

  if( asset_type == eUnitBlock ) {
   block_indices.push_back( { asset_index , i } );
  }
  else if( asset_type == eLine ) {
   line_indices.push_back( { asset_index , i } );
  }
  else {
   throw( std::logic_error( "InvestmentFunction::update_linearization: invalid"
                            " asset type: " + std::to_string( asset_type ) ) );
  }
 } // end( for each asset )

 for( Index stage = 0 ; stage < num_stages ; ++stage ) {

  retrieve_dual_solution( stage );

  if( ! block_indices.empty() )
   // The primal solution may only be necessary if there are UnitBlocks
   // subject to investment.
   retrieve_var_solution( stage );

  update_linearization_unit_blocks( stage , block_indices );
  update_linearization_network_blocks( stage , line_indices );
 } // end( for each stage )

}  // end( InvestmentFunction::update_linearization() )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::update_unit_block( UnitBlock * block ,
                                            double investment ) {
 if( dynamic_cast< const ThermalUnitBlock * >( block ) ) {
  block->scale( investment );
 }
 else if( auto unit = dynamic_cast< BatteryUnitBlock * >( block ) ) {
  if( f_scale_battery )
   unit->scale( investment );
  else
   unit->set_kappa( investment );
 }
 else if( auto unit = dynamic_cast< IntermittentUnitBlock * >( block ) ) {
  if( f_scale_intermittent )
   unit->scale( investment );
  else
   unit->set_kappa( investment );
 }
 else {
  // Unrecognized UnitBlock
  auto error_message = "InvestmentFunction::update_unit_block: "
   "unrecognized UnitBlock: " + block->classname();
  if( ! block->name().empty() )
   error_message += " with name '" + block->name() + "'";
  error_message += ".";
  throw( std::logic_error( error_message ) );
 }
}

/*--------------------------------------------------------------------------*/

void InvestmentFunction::update_unit_blocks
( const std::vector< Index > & block_indices ,
  const std::vector< double > & investment ) {

 assert( block_indices.size() == investment.size() );

 if( block_indices.empty() )
  return;

 const auto sddp_block = static_cast< SDDPBlock * >( v_Block.front() );
 const auto num_stages = sddp_block->get_time_horizon();

 for( Index stage = 0 ; stage < num_stages ; ++stage ) {
  auto ucblock = get_ucblock( stage );
  for( Index i = 0 ; i < block_indices.size() ; ++i ) {
   auto block = ucblock->get_unit_block( block_indices[ i ] );
   update_unit_block( block , investment[ i ] );
  } // end( for each UnitBlock )
 } // end( for each stage )
} // end( InvestmentFunction::update_unit_blocks )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::update_network_blocks
( const std::vector< Index > & line_indices ,
  const std::vector< double > & investment ) {

 assert( line_indices.size() == investment.size() );

 if( line_indices.empty() )
  return;

 const auto sddp_block = static_cast< SDDPBlock * >( v_Block.front() );
 const auto num_stages = sddp_block->get_time_horizon();

 for( Index stage = 0 ; stage < num_stages ; ++stage ) {

  auto ucblock = get_ucblock( stage );
  const auto time_horizon = ucblock->get_time_horizon();

  for( Index t = 0 ; t < time_horizon ; ++t ) {

   auto network_block = ucblock->get_network_block( t );

   if( auto dc_network = dynamic_cast< DCNetworkBlock * >( network_block ) ) {
    auto subset = line_indices;
    dc_network->set_kappa( investment.cbegin() , std::move( subset ) );
   }
   else {
    // Unrecognized NetworkBlock
    auto error_message = "InvestmentFunction::update_network_blocks: "
     "unrecognized NetworkBlock: " + network_block->classname() + ".";
    throw( std::logic_error( error_message ) );
   }
  } // end( for each time instant )
 } // end( for each stage )
} // end( InvestmentFunction::update_network_blocks )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::update_blocks() {

 const auto saved_f_ignore_modifications = f_ignore_modifications;

 f_ignore_modifications = true;

 // The indices of the UnitBlocks
 std::vector< Index > block_indices;
 block_indices.reserve( v_asset_indices.size() );

 // The investment to be made in the UnitBlocks
 std::vector< double > block_investment;
 block_investment.reserve( v_asset_indices.size() );

 // The indices of the transmission lines
 std::vector< Index > line_indices;
 line_indices.reserve( v_asset_indices.size() );

 // The investment to be made in the transmission lines
 std::vector< double > line_investment;
 line_investment.reserve( v_asset_indices.size() );

 for( Index i = 0 ; i < v_asset_indices.size() ; ++i ) {

  const auto asset_type =  v_asset_type[ i ];
  const auto asset_index =  v_asset_indices[ i ];
  const auto var_value = get_var_value( i , false );

  if( asset_type == eUnitBlock ) {
   block_indices.push_back( asset_index );
   block_investment.push_back( var_value );
  }
  else if( asset_type == eLine ) {
   line_indices.push_back( asset_index );
   line_investment.push_back( var_value );
  }
  else {
   throw( std::logic_error( "InvestmentFunction::update_blocks: invalid asset"
                            " type: " + std::to_string( asset_type ) ) );
  }
 } // end( for each asset )

 update_unit_blocks( block_indices , block_investment );
 update_network_blocks( line_indices , line_investment );

 f_ignore_modifications = saved_f_ignore_modifications;
 f_blocks_are_updated = true;
}  // end( InvestmentFunction::update_blocks )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::send_nuclear_modification
( const Observer::ChnlName chnl ) {
 // "nuclear modification" for Function: everything changed
 global_pool.invalidate();
 f_blocks_are_updated = false;
 generator_node_map.clear(); // the generator map must be rebuilt
 if( f_Observer )
  f_Observer->add_Modification
   ( std::make_shared<FunctionMod>( this , FunctionMod::NaNshift ) , chnl );
}  // end( InvestmentFunction::send_nuclear_modification )


/*--------------------------------------------------------------------------*/
/*----------------------------- GlobalPool ---------------------------------*/
/*--------------------------------------------------------------------------*/

void InvestmentFunction::GlobalPool::resize( Index size ) {
 linearization_coefficients.resize( size , {} );
 linearization_constants.resize( size , NaN );
 is_diagonal.resize( size );
}  // end( InvestmentFunction::GlobalPool::resize )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::GlobalPool::store
( FunctionValue constant , std::vector< FunctionValue > coefficients ,
  Index name , bool diagonal_linearization ) {
 if( name >= size() )
  throw( std::invalid_argument( "InvestmentFunction::GlobalPool::store: "
                                "invalid linearization name." ) );
 linearization_coefficients[ name ] = coefficients;
 linearization_constants[ name ] = constant;
 is_diagonal[ name ] = diagonal_linearization;
}  // end( InvestmentFunction::GlobalPool::store )

/*--------------------------------------------------------------------------*/

bool InvestmentFunction::GlobalPool::is_linearization_there( Index name )
 const {

 if( name >= size() || std::isnan( linearization_constants[ name ] ) )
  return false;
 return true;
}  // end( InvestmentFunction::GlobalPool::is_linearization_there )

/*--------------------------------------------------------------------------*/

bool InvestmentFunction::GlobalPool::is_linearization_vertical( Index name )
 const {

 if( name >= size() || std::isnan( linearization_constants[ name ] ) )
  return false;
 return( ! is_diagonal[ name ] );
}  // end( InvestmentFunction::GlobalPool::is_linearization_vertical )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::GlobalPool::store_combination_of_linearizations
( c_LinearCombination & linear_combination , Index name ,
  FunctionValue AAccMlt ) {

 if( name >= size() )
  throw( std::invalid_argument
         ( "InvestmentFunction::GlobalPool::store_combination_of_"
           "linearizations: invalid global pool name." ) );

 if( linear_combination.empty() )
  throw( std::invalid_argument
         ( "InvestmentFunction::GlobalPool::store_combination_of_"
           "linearizations: linear combination is empty." ) );

 const auto combine = []( std::vector< FunctionValue > & x ,
                          const std::vector< FunctionValue > & y ,
                          const FunctionValue multiplier ) {
  assert( x.size() == y.size() );
  for( Index i = 0 ; i < x.size() ; ++i )
   x[ i ] += multiplier * y[ i ];
 };

 bool diagonal_linearization = false;
 std::vector< FunctionValue > coefficients;
 FunctionValue constant = 0;
 FunctionValue coeff_sum_diagonal = 0;

 for( const auto name_coeff : linear_combination ) {
  const auto linearization_name = name_coeff.first;
  const auto coeff = name_coeff.second;

  if( coeff < - AAccMlt ) {
   throw( std::invalid_argument
          ( "InvestmentFunction::GlobalPool::store_combination_of_"
            "linearizations: invalid coefficient for linearization with name " +
            std::to_string( linearization_name ) + ": " +
            std::to_string( coeff ) ) );
  }

  if( coefficients.empty() )
   coefficients.resize
    ( linearization_coefficients[ linearization_name ].size() , 0 );
  else
   combine( coefficients , linearization_coefficients[ linearization_name ] ,
            coeff );

  constant += coeff * linearization_constants[ linearization_name ];

  if( is_diagonal[ linearization_name ] ) {
   coeff_sum_diagonal += coeff;
   diagonal_linearization = true;
  }
 }

 if( diagonal_linearization &&
     std::abs( FunctionValue( 1 ) - coeff_sum_diagonal ) >
     AAccMlt * linear_combination.size() ) {

  throw( std::invalid_argument
         ( "InvestmentFunction::GlobalPool::store_combination_of_"
           "linearizations: a non-convex combination of diagonal "
           "linearizations has been provided." ) );
 }

 this->store( constant , coefficients , name , diagonal_linearization );

} // end( InvestmentFunction::GlobalPool::store_combination_of_linearizations )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::GlobalPool::delete_linearization( const Index name ) {
 if( name >= size() )
  throw( std::invalid_argument( "GlobalPool::delete_linearization: invalid "
                                "linearization name: " +
                                std::to_string( name ) ) );

 linearization_constants[ name ] = NaN;
 linearization_coefficients[ name ] = {};
}  // end( InvestmentFunction::GlobalPool::delete_linearization )

/*--------------------------------------------------------------------------*/

void InvestmentFunction::GlobalPool::delete_linearizations( Subset & which ,
                                                            bool ordered ) {
 if( which.empty() ) {  // delete them all
  for( Index i = 0 ; i < size() ; ++i )
   if( is_linearization_there( i ) )
    delete_linearization( i );
 }
 else {                 // delete the given subset
  if( ! ordered )
   std::sort( which.begin() , which.end() );

  if( which.back() >= size() )
   throw( std::invalid_argument( "InvestmentFunction::GlobalPool::delete_linea"
                                 "rizations: invalid linearization name." ) );

  for( auto i : which )
   if( is_linearization_there( i ) )
    delete_linearization( i );
 }
}

/*--------------------------------------------------------------------------*/
/*-------------------- End File InvestmentFunction.cpp ---------------------*/
/*--------------------------------------------------------------------------*/
