/*--------------------------------------------------------------------------*/
/*---------------------- File UCBlockSolutionOutput.h ----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * The UCBlockSolutionOutput is a convenient class to export the solution of a
 * UCBlock in the form of a CSV file.
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

#ifndef __UCBlockSolutionOutput
#define __UCBlockSolutionOutput

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "BatteryUnitBlock.h"
#include "DCNetworkBlock.h"
#include "HydroSystemUnitBlock.h"
#include "IntermittentUnitBlock.h"
#include "SlackUnitBlock.h"
#include "ThermalUnitBlock.h"
#include "UCBlock.h"

#include <iostream>

/*--------------------------------------------------------------------------*/
/*--------------------------- NAMESPACE ------------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*---------------------- CLASS UCBlockSolutionOutput -----------------------*/
/*--------------------------------------------------------------------------*/

class UCBlockSolutionOutput {

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

 template<class T>
 void print( std::ostream & output , T * ) const;

/*--------------------------------------------------------------------------*/

 template<>
 void print( std::ostream & output , BatteryUnitBlock * block ) const {

  print_basic_info( output , block );
  print_unit_block_data( output , block );

  // storage level
  print_array( output , block->get_storage_level() );

  if( full_output ) {
   // intake level
   print_rounded_array( output , block->get_intake_level() );

   // outtake level
   print_rounded_array( output , block->get_outtake_level() );

   // battery binary
   print_rounded_array( output , block->get_battery_binary() );
  }
 }

/*--------------------------------------------------------------------------*/

 template<>
 void print( std::ostream & output , HydroUnitBlock * block ) const {

  print_basic_info( output , block );
  print_unit_block_data( output , block );

  const auto time_horizon = block->get_time_horizon();

  if( full_output ) {
   const auto number_generators = block->get_number_generators();

   // flow rate
   for( Index l = 0 ; l < number_generators ; ++l )
    print_array( output , block->get_flow_rate( l ) , time_horizon );
  }

  output << block->get_number_reservoirs() << std::endl;

  // volumes
  for( Index r = 0 ; r < block->get_number_reservoirs() ; ++r )
   print_array( output , block->get_volumetric( r ) , time_horizon );
 }

/*--------------------------------------------------------------------------*/

 template<>
 void print( std::ostream & output , HydroSystemUnitBlock * block ) const {
  for( Index h = 0 ; h < block->get_number_hydro_units() ; ++h ) {
   auto hydro_unit_block = block->get_hydro_unit_block( h );
   if( ! hydro_unit_block ) continue;
   print( output , hydro_unit_block );
  }
 }

/*--------------------------------------------------------------------------*/

 template<>
 void print( std::ostream & output , IntermittentUnitBlock * block ) const {

  print_basic_info( output , block );
  print_unit_block_data( output , block );
 }

/*--------------------------------------------------------------------------*/

 template<>
 void print( std::ostream & output , NetworkBlock * block ) const {

  print_basic_info( output , block );

  // number of nodes and number of lines
  if( auto network_data = block->get_NetworkData() )
   output << network_data->get_number_nodes() << separator_character
          << network_data->get_number_lines() << std::endl;
  else if( dynamic_cast<BusNetworkBlock *>( block ) )
   output << 1 << separator_character << 0 << std::endl;
  else
   output << 0 << separator_character << 0 << std::endl;

  // node injection
  print_array( output , block->get_node_injection() );

  // power flow
  if( auto dc_network_block = dynamic_cast<DCNetworkBlock *>( block ) ) {
   print_array( output , dc_network_block->get_power_flow() );
  }
 }

/*--------------------------------------------------------------------------*/

 template<>
 void print( std::ostream & output , SlackUnitBlock * block ) const {
  print_basic_info( output , block );
  print_unit_block_data( output , block );
 }

/*--------------------------------------------------------------------------*/

 template<>
 void print( std::ostream & output , ThermalUnitBlock * block ) const {

  print_basic_info( output , block );
  print_unit_block_data( output , block );

  if( full_output ) {
   // start up
   print_rounded_array( output , block->get_start_up() );

   // shutdown
   print_rounded_array( output , block->get_shut_down() );
  }
 }

/*--------------------------------------------------------------------------*/

 template<>
 void print( std::ostream & output , UCBlock * uc_block ) const {

  print_name( output , uc_block );

  output << uc_block->get_time_horizon() << separator_character
         << count_blocks( uc_block ) << std::endl;

  for( auto b : uc_block->get_nested_Blocks() ) {
   if( auto block = dynamic_cast<BatteryUnitBlock *>( b ) )
    print( output , block );
   else if( auto block = dynamic_cast<HydroSystemUnitBlock *>( b ) )
    print( output , block );
   else if( auto block = dynamic_cast<HydroUnitBlock *>( b ) )
    print( output , block );
   else if( auto block = dynamic_cast<IntermittentUnitBlock *>( b ) )
    print( output , block );
   else if( auto block = dynamic_cast<NetworkBlock *>( b ) )
    print( output , block );
   else if( auto block = dynamic_cast<SlackUnitBlock *>( b ) )
    print( output , block );
   else if( auto block = dynamic_cast<ThermalUnitBlock *>( b ) )
    print( output , block );
  }
 } // end( print( UCBlock * ) )

/*--------------------------------------------------------------------------*/

 void set_separator_character( char separator_character ) {
  this->separator_character = separator_character;
 }

/*--------------------------------------------------------------------------*/

 void set_full_output( bool full_output ) {
  this->full_output = full_output;
 }

/*--------------------------------------------------------------------------*/
/*--------------------- PRIVATE PART OF THE CLASS --------------------------*/
/*--------------------------------------------------------------------------*/

private:

/*--------------------------------------------------------------------------*/
/*--------------------------- PRIVATE METHODS ------------------------------*/
/*--------------------------------------------------------------------------*/

 Index count_blocks( Block * block ) const {
  Index num_blocks = 0;
  for( auto b : block->get_nested_Blocks() ) {
   if( auto sub_block = dynamic_cast<HydroSystemUnitBlock *>( b ) )
    num_blocks += count_blocks( sub_block );
   else if( ! dynamic_cast<PolyhedralFunctionBlock *>( b ) )
    ++num_blocks;
  }
  return num_blocks;
 }

/*--------------------------------------------------------------------------*/

 void print_rounded_array( std::ostream & output , const ColVariable * array ,
                           const std::size_t size ) const {
  for( Index i = 0 ; i < size && array ; ++i , ++array ) {
   if( i > 0 )
    output << separator_character;
   output << ( unsigned int ) std::round( array->get_value() );
  }
  output << std::endl;
 }

/*--------------------------------------------------------------------------*/

 void print_rounded_array( std::ostream & output ,
                           const std::vector< ColVariable > & array ) const {
  print_rounded_array( output , array.data() , array.size() );
 }

/*--------------------------------------------------------------------------*/

 void print_array( std::ostream & output , const ColVariable * array ,
                   const std::size_t size , const int precision = 15 ) const {
  for( Index i = 0 ; i < size && array ; ++i , ++array ) {
   if( i > 0 )
    output << separator_character;

   output << std::setprecision( precision ) << array->get_value();
  }
  output << std::endl;
 }

/*--------------------------------------------------------------------------*/

 void print_array( std::ostream & output ,
                   const std::vector< ColVariable > & array ,
                   const int precision = 15 ) const {
  print_array( output , array.data() , array.size() , precision );
 }

/*--------------------------------------------------------------------------*/

 void print_name( std::ostream & output , const Block * block  ) const {
  const auto name = block->name();
  if( ! name.empty() )
   output << name << std::endl;
  else
   output << block->classname() << std::endl;
 }

/*--------------------------------------------------------------------------*/

 void print_cost( std::ostream & output , const Block * block  ) const {
  if( auto obj = dynamic_cast<FRealObjective *>( block->get_objective() ) ) {
   auto status = obj->compute();
   assert( status == ThinComputeInterface::kOK );
   output << obj->value() << std::endl;
  }
  else
   output << 0 << std::endl;
 }

/*--------------------------------------------------------------------------*/

 void print_basic_info( std::ostream & output , const Block * block  ) const {
  print_name( output , block );
  print_cost( output , block );
 }

/*--------------------------------------------------------------------------*/

 /// print values of Variables that are common to all UnitBlock
 void print_unit_block_data( std::ostream & output ,
                             UnitBlock * block ) const {

  const auto time_horizon = block->get_time_horizon();
  const auto number_generators = block->get_number_generators();

  output << number_generators << std::endl;

  // commitment
  for( Index g = 0 ; g < number_generators ; ++g )
   print_array( output , block->get_commitment( g ) , time_horizon );

  // active power
  for( Index g = 0 ; g < number_generators ; ++g )
   print_array( output , block->get_active_power( g ) , time_horizon );

  // primary spinning reserve
  for( Index g = 0 ; g < number_generators ; ++g )
   print_array( output , block->get_primary_spinning_reserve( g ) ,
                time_horizon );

  // secondary spinning reserve
  for( Index g = 0 ; g < number_generators ; ++g )
   print_array( output , block->get_secondary_spinning_reserve( g ) ,
                time_horizon );
 }

/*--------------------------------------------------------------------------*/
/*---------------------------- PRIVATE FIELDS  -----------------------------*/
/*--------------------------------------------------------------------------*/

 char separator_character = ',';
 bool full_output = false;

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

};  // end( class UCBlockSolutionOutput )

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

#endif  /* UCBlockSolutionOutput.h included */

/*--------------------------------------------------------------------------*/
/*-------------------- End File UCBlockSolutionOutput.h --------------------*/
/*--------------------------------------------------------------------------*/
