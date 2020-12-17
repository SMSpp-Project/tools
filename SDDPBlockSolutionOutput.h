/*--------------------------------------------------------------------------*/
/*--------------------- File SDDPBlockSolutionOutput.h ---------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * \version 0.1
 *
 * \date 08 - 10 - 2020
 *
 * \author Rafael Durbano Lobato \n
 *         Operations Research Group \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Rafael Durbano Lobato
 */

/*--------------------------------------------------------------------------*/
/*----------------------------- DEFINITIONS --------------------------------*/
/*--------------------------------------------------------------------------*/

#ifndef __SDDPBlockSolutionOutput
#define __SDDPBlockSolutionOutput

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "SDDPBlock.h"
#include "UCBlockSolutionOutput.h"

#include <iostream>

/*--------------------------------------------------------------------------*/
/*--------------------------- NAMESPACE ------------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*--------------------- CLASS SDDPBlockSolutionOutput ----------------------*/
/*--------------------------------------------------------------------------*/

class SDDPBlockSolutionOutput {

/*--------------------------------------------------------------------------*/
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/
/*--------------------------------------------------------------------------*/

public:

/*--------------------------------------------------------------------------*/
/*---------------------------- PUBLIC TYPES --------------------------------*/
/*--------------------------------------------------------------------------*/

 using Index = Block::Index;

/*--------------------------------------------------------------------------*/
/*--------------------- PUBLIC METHODS OF THE CLASS ------------------------*/
/*--------------------------------------------------------------------------*/

 void print( SDDPBlock * block ) const {

  UCBlockSolutionOutput solution_output;
  solution_output.set_separator_character( separator_character );

  Index initial_time = 0;

  for( Index stage = 0 ; stage < block->get_time_horizon() ; ++stage ) {

   auto benders_block = static_cast< BendersBlock * >
    ( static_cast< StochasticBlock * >( block->get_nested_Blocks()[ stage ] )->
      get_nested_Blocks().front() );

   std::cout << "benders " << benders_block << std::endl;

   auto objective = static_cast< FRealObjective * >
    ( benders_block->get_objective() );

   std::cout << "obj " << objective << std::endl;

   auto benders_function = static_cast< BendersBFunction * >
    ( objective->get_function() );

   std::cout << "fun " << benders_function << std::endl;

   auto uc_block = static_cast< UCBlock * >
    ( benders_function->get_inner_block() );

   std::cout << "uc_block " << uc_block << std::endl;

   auto solver = benders_function->get_solver<CDASolver>();

   std::cout << "solver " << solver << std::endl;

   assert( solver->has_var_solution() );
   if( solver->has_var_solution() )
    solver->get_var_solution();
   if( solver->has_dual_solution() )
    solver->get_dual_solution();

   std::cout << "printing UCBlock " << uc_block << std::endl;
   solution_output.print( uc_block );
   std::cout << "done" << std::endl;

   solution_output.set_append();
   initial_time += uc_block->get_time_horizon();
   solution_output.set_initial_time( initial_time );
  }
 }

/*--------------------------------------------------------------------------*/


 void print( SDDPBlock * block , Index scenario , bool append ) const {

  UCBlockSolutionOutput solution_output;
  solution_output.set_separator_character( separator_character );

  Index initial_inner_time = 0;

  for( Index stage = 0 ; stage < block->get_time_horizon() ; ++stage ) {

   auto benders_block = static_cast< BendersBlock * >
    ( static_cast< StochasticBlock * >( block->get_nested_Blocks()[ stage ] )->
      get_nested_Blocks().front() );

   auto objective = static_cast< FRealObjective * >
    ( benders_block->get_objective() );

   auto benders_function = static_cast< BendersBFunction * >
    ( objective->get_function() );

   auto uc_block = static_cast< UCBlock * >
    ( benders_function->get_inner_block() );

   if( ! append ) {
    solution_output.set_append( false );
    solution_output.set_filenames_suffix
     ( "_Scen" + std::to_string( scenario ) + "_" + std::to_string( stage )
       + "_OUT.csv" );
   }
   else if( append ) {
    solution_output.set_filenames_suffix( "_Scen" + std::to_string( scenario ) +
                                          "_OUT.csv" );
    if( stage == 0 )
     solution_output.set_append( false );
    else
     solution_output.set_append( true );
   }

   solution_output.print( uc_block );

   initial_inner_time += uc_block->get_time_horizon();
   solution_output.set_initial_time( initial_inner_time );
  }
 }

/*--------------------------------------------------------------------------*/

 void set_separator_character( char separator_character ) {
  this->separator_character = separator_character;
 }

/*--------------------------------------------------------------------------*/
/*--------------------- PRIVATE PART OF THE CLASS --------------------------*/
/*--------------------------------------------------------------------------*/

private:

/*--------------------------------------------------------------------------*/
/*---------------------------- PRIVATE FIELDS  -----------------------------*/
/*--------------------------------------------------------------------------*/

 char separator_character = ',';
 bool append = false;

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

};  // end( class SDDPBlockSolutionOutput )

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

#endif  /* SDDPBlockSolutionOutput.h included */

/*--------------------------------------------------------------------------*/
/*------------------- End File SDDPBlockSolutionOutput.h -------------------*/
/*--------------------------------------------------------------------------*/
