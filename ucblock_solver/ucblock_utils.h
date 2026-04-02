/*--------------------------------------------------------------------------*/
/*-------------------------- File ucblock_utils.h --------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Utilities for the UC solver.
 *
 * \author Ali Ghezelsoflu \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Niccolo' Iardella \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Ali Ghezelsoflu, Niccolo' Iardella
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <DesignNetworkBlock.h>
#include <DCNetworkBlock.h>
#include <HydroSystemUnitBlock.h>
#include <SlackUnitBlock.h>
#include <RBlockConfig.h>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/// Returns a default UCBlock configuration

inline BlockConfig * default_configure_UCBlock( const Block * uc_block )
{
 auto b_config = new RBlockConfig;

 for( auto sb : uc_block->get_nested_Blocks() ) {
  if( ! dynamic_cast< UnitBlock * >( sb ) )
   continue;

  auto sbc = new RBlockConfig;

  // If HydroSystemUnitBlock, we configure its PolyhedralFunctionBlocks
  if( auto hsub = dynamic_cast< HydroSystemUnitBlock * >( sb ) ) {
   for( auto ssb : hsub->get_nested_Blocks() ) {
    if( auto pf_block = dynamic_cast< PolyhedralFunctionBlock * >( ssb ) ) {
     auto ssbc = new BlockConfig();
     ssbc->f_static_variables_Configuration =
      new SimpleConfiguration< int >( 1 );

     int idx = sb->get_nested_Block_index( ssb );
     sbc->add_sub_BlockConfig( ssbc , idx );
     }
    }
   }

  int idx = uc_block->get_nested_Block_index( sb );
  b_config->add_sub_BlockConfig( sbc , idx );
  }

 return( b_config );
 }

/*--------------------------------------------------------------------------*/
/*------------------------ End File ucblock_utils.h ------------------------*/
/*--------------------------------------------------------------------------*/
