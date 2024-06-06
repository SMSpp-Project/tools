/*--------------------------------------------------------------------------*/
/*------------------------- File CutProcessing.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of CutProcessing.
 *
 * \author Rafael Durbano Lobato \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Rafael Durbano Lobato
 */

/*--------------------------------------------------------------------------*/
/*---------------------------- IMPLEMENTATION ------------------------------*/
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <AbstractBlock.h>
#include <BlockSolverConfig.h>
#include <ColVariable.h>
#include <FRealObjective.h>
#include <FRowConstraint.h>
#include <LinearFunction.h>
#include <PolyhedralFunction.h>
#include <SDDPBlock.h>

#include "CutProcessing.h"

#include <list>
#include <vector>

/*--------------------------------------------------------------------------*/
/*------------------------- NAMESPACE AND USING ----------------------------*/
/*--------------------------------------------------------------------------*/

using Index = Block::Index;
using Subset = Block::Subset;

/*--------------------------------------------------------------------------*/
/*--------------------------- AUXILIARY METHODS ----------------------------*/
/*--------------------------------------------------------------------------*/

namespace {

 double get_sign( PolyhedralFunction * function ) {
  return( function->is_convex() ? - 1.0 : 1.0 );
  }

/*--------------------------------------------------------------------------*/
 /** Given a pointer to a PolyhedralFunction representing the function
  *
  *     f( x ) = max { a_i x + b_i : i = 0, ... , m - 1 }
  *
  * in the convex case (pointwise maximum of linear functions), and
  *
  *     f( x ) = min { a_i x + b_i : i = 0, ... , m - 1 }
  *
  * in the concave case (pointwise minimum of linear functions), it constructs
  * an AbstractBlock representing the problem
  *
  *     max   s * ( y - a_0 x )
  *     s.t.  s * ( y - a_i x ) <= s * b_i , i = 1, ..., m - 1
  *
  * where s = - 1 in the convex case and s = 1 in the concave case.
  */

 AbstractBlock * build_lp( PolyhedralFunction * function ) {

  const auto & A = function->get_A();

  if( A.empty() )
   return( nullptr );

  const auto & b = function->get_b();

  assert( A.size() == b.size() );

  auto lp = new AbstractBlock;
  const auto num_var = A.front().size();

  auto x = new std::vector< ColVariable >( num_var );
  lp->add_static_variable( * x , "x" );

  auto y = new ColVariable;
  lp->add_static_variable( * y , "y" );

  const auto sign = get_sign( function );

  auto objective_function = new LinearFunction;
  for( Index j = 0 ; j < x->size() ; ++j )
   objective_function->add_variable( & ( *x )[ j ] , - sign * A[ 0 ][ j ] );
  objective_function->add_variable( y , sign );

  auto objective = new FRealObjective( lp , objective_function );
  objective->set_sense( Objective::eMax );

  lp->set_objective( objective );

  auto constraints = new std::list< FRowConstraint >( A.size() - 1 );
  auto constraints_it = constraints->begin();

  for( Index i = 1 ; i < A.size() ; ++i ) {
   auto function = new LinearFunction;
   for( Index j = 0 ; j < x->size() ; ++j ) {
    function->add_variable( & ( *x )[ j ] , - sign * A[ i ][ j ] );
   }
   function->add_variable( y , sign );
   ( * constraints_it ).set_lhs( -Inf< double >() );
   ( * constraints_it ).set_rhs( sign * b[ i ]  );
   ( * constraints_it ).set_function( function );
   constraints_it++;
  }

  lp->add_dynamic_constraint( * constraints , "c" );

  return( lp );
 }

/*--------------------------------------------------------------------------*/

 /** It updates the given LP problem by possibly moving the cut associated
  * with the objective into the set of constraints and moving the first
  * constraint of the LP into the objective.
  *
  * @param lp A pointer to the LP problem.
  *
  * @param function A pointer to the PolyhedralFunction being considered.
  *
  * @param i Index of the cut that is currently in the objective.
  *
  * @param move_objective_to_constraint Indicates whether the cut currently in
  *        the objective must be added to the constraints. If this parameter
  *        is false, this cut is eliminated from the LP.
  */
 void update_lp( AbstractBlock * lp , PolyhedralFunction * function ,
                 Index i , bool move_objective_to_constraint = false ) {

  auto constraints = lp->get_dynamic_constraint< FRowConstraint >( 0 );

  assert( ! ( * constraints ).empty() );

  auto objective = static_cast< FRealObjective * >( lp->get_objective() );
  auto objective_function =
   static_cast< LinearFunction * >( objective->get_function() );

  if( move_objective_to_constraint && i > 0 ) {

   // add the cut currently in the objective to the end of the list of
   // constraints

   const auto sign = get_sign( function );
   const auto & b = function->get_b();

   std::list< FRowConstraint > new_constraint( 1 );
   new_constraint.front().set_lhs( -Inf< double >() );
   new_constraint.front().set_rhs( sign * b[ i ]  );

   auto v_var = objective_function->get_v_var();
   new_constraint.front().set_function
    ( new LinearFunction( std::move( v_var ) ) );

   lp->add_dynamic_constraints( * constraints , new_constraint );
  }

  // put the cut of the first constraint in the objective

  auto & first_constraint = ( * constraints ).front();

  auto constraint_function =
   static_cast< LinearFunction * >( first_constraint.get_function() );

  for( Index j = 0 ; j < constraint_function->get_num_active_var() ; ++j ) {
   objective_function->modify_coefficient
    ( j , constraint_function->get_coefficient( j ) );
  }

  // remove the first constraint

  lp->remove_dynamic_constraint( * constraints , ( * constraints ).begin() );

 }

}  // end( unnamed namespace )

/*--------------------------------------------------------------------------*/
/*------------------------- METHODS of CutProcessing -----------------------*/
/*--------------------------------------------------------------------------*/

void CutProcessing::remove_parallel_cuts( PolyhedralFunction * function )
 const {

 const auto & A = function->get_A();

 if( A.empty() )
  return;

 const auto & b = function->get_b();

 assert( A.size() == b.size() );

 const auto sign = get_sign( function );

 Subset rows_to_remove;
 std::vector< bool > remove( A.size() , false );

 for( Index i = 0 ; i < A.size() ; ++i ) {
  if( remove[ i ] ) continue;

  for( Index k = i + 1 ; k < A.size() ; ++k ) {
   if( remove[ k ] ) continue;

   assert( A[ i ].size() == A[ k ].size() );

   bool parallel = true;

   for( Index j = 0 ; j < A[ i ].size() ; ++j ) {
    if( ( std::abs( A[ i ][ j ] - A[ k ][ j ] ) > parallel_error ) ||
        ( std::abs( A[ i ][ j ] - A[ k ][ j ] ) > parallel_relative_error *
          std::max( std::abs( A[ i ][ j ] ) , std::abs( A[ k ][ j ] ) ) ) ) {
     parallel = false;
     break;
    }
   }

   if( parallel ) {
    if( sign * b[ i ] > sign * b[ k ] ) {
     remove[ i ] = true;
     rows_to_remove.push_back( i );
     break;
    }
    else {
     remove[ k ] = true;
     rows_to_remove.push_back( k );
    }
   }
  }
 }

 function->delete_rows( std::move( rows_to_remove ) );
}

/*--------------------------------------------------------------------------*/

void CutProcessing::remove_redundant_cuts( PolyhedralFunction * function )
 const {

 auto num_rows = function->get_nrows();

 if( num_rows <= 1 )
  return;

 auto lp = ::build_lp( function );

 if( config_filename.empty() ) {
  auto bsc = new BlockSolverConfig();
  auto cc = new ComputeConfig();
  cc->set_par( "intLogVerb" , int( 0 ) );
  cc->set_par( "dblFAccSol" , double( 1.0e-15 ) );
  cc->set_par( "dblRelAcc" , double( 1.0e-15 ) );
  bsc->add_ComputeConfig( "CPXMILPSolver" , cc );
  bsc->apply( lp );
 }
 else {
  auto solver_config = dynamic_cast< BlockSolverConfig * >
   ( BlockSolverConfig::new_Configuration( config_filename ) );
  if( ! solver_config )
   throw( std::logic_error("CutProcessing::remove_redundant_cuts: invalid or "
                           "inexistent configuration file: " +
                           config_filename ) );
  solver_config->apply( lp );
 }

 auto solver = lp->get_registered_solvers().front();

 num_rows = function->get_nrows();

 Subset rows_to_remove;
 rows_to_remove.reserve( num_rows - 1 );

 const auto sign = get_sign( function );
 const auto & b = function->get_b();

 for( Index i = 0 ; i < function->get_nrows(); ++i ) {

  bool remove = false;

  if( solver->compute() == Solver::kOK ) {
   const auto objective_value = solver->get_var_value();
   if( ( objective_value < sign * b[ i ] + optimization_error ) &&
       ( objective_value < sign * b[ i ] + optimization_relative_error *
         std::max( std::abs( objective_value ) , std::abs( b[ i ] ) ) ) ) {
    remove = true;
    rows_to_remove.push_back( i );
   }
  }

  if( i < num_rows - 1 )
   ::update_lp( lp , function , i , ! remove );
 }

 function->delete_rows( std::move( rows_to_remove ) );

 delete( solver );
 delete( lp );
}

/*--------------------------------------------------------------------------*/

void CutProcessing::remove_parallel_cuts( SDDPBlock * sddp_block ) const {
 auto functions = sddp_block->get_polyhedral_functions();
 for( auto function : functions ) {
  remove_parallel_cuts( function );
 }
}

/*--------------------------------------------------------------------------*/

void CutProcessing::remove_redundant_cuts( SDDPBlock * sddp_block ) const {
 auto functions = sddp_block->get_polyhedral_functions();
 for( auto function : functions ) {
  remove_parallel_cuts( function );
  remove_redundant_cuts( function );
 }
}

/*--------------------------------------------------------------------------*/
/*--------------------- End File CutProcessing.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
