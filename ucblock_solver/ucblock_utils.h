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

#include <BatteryUnitBlock.h>
#include <CDASolver.h>
#include <DCNetworkBlock.h>
#include <ECNetworkBlock.h>
#include <HydroSystemUnitBlock.h>
#include <HydroUnitBlock.h>
#include <SlackUnitBlock.h>
#include <IntermittentUnitBlock.h>
#include <ThermalUnitBlock.h>

#include "UCBlockSolutionOutput.h"

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/

/// Returns a default UCBlock configuration
BlockConfig * default_configure_UCBlock( Block * uc_block ) {

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

/// Prints the content of a solved UCBlock
void print_UCBlock_solver_results( Block * block ) {

 auto solver = block->get_registered_solvers().front();
 solver->get_var_solution();

 int n_unit_blocks = 0;
 int n_net_blocks = 0;

 std::cout << std::endl;

 for( auto i : block->get_nested_Blocks() ) {

  auto uc_block = dynamic_cast< UCBlock * >( block );

  Index number_primary_zones = uc_block->get_number_primary_zones();
  Index number_secondary_zones = uc_block->get_number_secondary_zones();
  Index number_inertia_zones = uc_block->get_number_inertia_zones();

  if( auto unit_block = dynamic_cast< UnitBlock * >( i ) ) {
   std::cout << "----- " << unit_block->classname() <<
             " " << n_unit_blocks++ << " -----" << std::endl;

   if( auto obj =
    dynamic_cast< FRealObjective * >( unit_block->get_objective() ) ) {
    auto fun = obj->get_function();
    fun->compute();
    std::cout << "Function value = " << fun->get_value() << std::endl;
   }

   if( auto thermal_unit_block =
    dynamic_cast< ThermalUnitBlock * >( unit_block ) ) {

    if( thermal_unit_block->get_investment_cost() != 0 )
     std::cout << "Capacity       = " <<
               thermal_unit_block->get_design().get_value() *
               thermal_unit_block->get_capacity() << std::endl;

    auto commitment = thermal_unit_block->get_commitment( 0 );
    std::cout << "Commitment     = [";
    for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
     std::cout << std::setw( 2 )
               << ( unsigned int ) round( commitment[ t ].get_value() );
    std::cout << " ]" << std::endl;

    // Generate init_t
    Index init_t;
    auto init_up_down_time = thermal_unit_block->get_init_up_down_time();
    auto min_up_time = thermal_unit_block->get_min_up_time();
    auto min_down_time = thermal_unit_block->get_min_down_time();
    if( init_up_down_time > 0 )
     init_t = ( init_up_down_time >= min_up_time ?
                0 : min_up_time - init_up_down_time );
    else
     init_t = ( -init_up_down_time >= min_down_time ?
                0 : min_down_time + init_up_down_time );

    auto startup = thermal_unit_block->get_start_up();
    std::cout << "Start up       = [";
    for( Index t = 0 ; t < unit_block->get_time_horizon() - init_t ; ++t )
     std::cout << std::setw( 2 )
               << ( unsigned int ) round( startup[ t ].get_value() );
    std::cout << " ]" << std::endl;

    auto shutdown = thermal_unit_block->get_shut_down();
    std::cout << "Shut down      = [";
    for( Index t = 0 ; t < unit_block->get_time_horizon() - init_t ; ++t )
     std::cout << std::setw( 2 )
               << ( unsigned int ) round( shutdown[ t ].get_value() );
    std::cout << " ]" << std::endl;

    auto active_power = thermal_unit_block->get_active_power( 0 );
    std::cout << "Active power   = [";
    for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
     std::cout << std::setw( 20 ) << active_power[ t ].get_value();
    std::cout << " ]" << std::endl;

    if( number_primary_zones > 0 ) {
     auto primary_reserve = thermal_unit_block
      ->get_primary_spinning_reserve( 0 );
     std::cout << "Primary reserve     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
      std::cout << std::setw( 20 ) << primary_reserve[ t ].get_value();
     std::cout << " ]" << std::endl;
    }

    if( number_secondary_zones > 0 ) {
     auto secondary_reserve = thermal_unit_block
      ->get_secondary_spinning_reserve( 0 );
     std::cout << "Secondary reserve     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
      std::cout << std::setw( 20 ) << secondary_reserve[ t ].get_value();
     std::cout << " ]" << std::endl;
    }
   }

   if( auto battery_unit_block =
    dynamic_cast< BatteryUnitBlock * >( unit_block ) ) {

    if( battery_unit_block->get_batt_investment_cost() != 0 )
     std::cout << "Batt Capacity  = " <<
               battery_unit_block->get_batt_design().get_value() *
               battery_unit_block->get_batt_max_capacity() << std::endl;

    if( battery_unit_block->get_conv_investment_cost() != 0 )
     std::cout << "Conv Capacity  = " <<
               battery_unit_block->get_conv_design().get_value() *
               battery_unit_block->get_conv_max_capacity() << std::endl;

    auto active_power = battery_unit_block->get_active_power( 0 );
    std::cout << "Active power   = [";
    for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
     std::cout << std::setw( 20 ) << active_power[ t ].get_value();
    std::cout << " ]" << std::endl;

    if( number_primary_zones > 0 ) {
     auto PrimarySR = battery_unit_block->get_primary_spinning_reserve( 0 );
     std::cout << "Primary reserve     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
      std::cout << std::setw( 20 ) << PrimarySR[ t ].get_value();
     std::cout << " ]" << std::endl;
    }
    if( number_secondary_zones > 0 ) {
     auto SecondarySR = battery_unit_block->get_secondary_spinning_reserve( 0 );
     std::cout << "Secondary reserve     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
      std::cout << std::setw( 20 ) << SecondarySR[ t ].get_value();
     std::cout << " ]" << std::endl;
    }

    auto intake_level = battery_unit_block->get_intake_level();
    std::cout << "Intake level   = [";
    for( auto & t : intake_level )
     std::cout << std::setw( 2 ) << ( unsigned int ) round( t.get_value() );
    std::cout << " ]" << std::endl;

    auto outtake_level = battery_unit_block->get_outtake_level();
    std::cout << "Outtake level  = [";
    for( auto & t : outtake_level )
     std::cout << std::setw( 2 ) << ( unsigned int ) round( t.get_value() );
    std::cout << " ]" << std::endl;

    auto storage_level = battery_unit_block->get_storage_level();
    std::cout << "Storage level  = [";
    for( auto & t : storage_level )
     std::cout << std::setw( 20 ) << t.get_value();
    std::cout << " ]" << std::endl;

    auto binary_var = battery_unit_block->get_intake_outtake_binary_variables();
    if( ! binary_var.empty() ) {
     std::cout << "Binary var   = [";
     for( auto & t : binary_var )
      std::cout << std::setw( 2 ) << ( unsigned int ) round( t.get_value() );
     std::cout << " ]" << std::endl;
    }
   }

   if( auto hydro_block = dynamic_cast< HydroUnitBlock * >( unit_block ) ) {
    for( Index g = 0 ; g < unit_block->get_number_generators() ; ++g ) {
     auto active_power = hydro_block->get_active_power( g );
     std::cout << "Active power [" + std::to_string( g ) + "]" " = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
      std::cout << std::setw( 20 ) << active_power[ t ].get_value();
     std::cout << " ]" << std::endl;
    }

    if( number_primary_zones > 0 ) {
     for( Index g = 0 ;
          g < unit_block->get_number_generators() ; ++g ) {
      auto primary_reserve = hydro_block->get_primary_spinning_reserve( g );
      std::cout << "Primary reserve [" + std::to_string( g ) + "]" " = [";
      for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
       std::cout << std::setw( 20 ) << primary_reserve[ t ].get_value();
      std::cout << " ]" << std::endl;
     }
    }
    if( number_secondary_zones > 0 ) {
     for( Index g = 0 ;
          g < unit_block->get_number_generators() ; ++g ) {
      auto secondary_reserve = hydro_block
       ->get_secondary_spinning_reserve( g );
      std::cout << "Secondary reserve [" + std::to_string( g ) + "]" " = [";
      for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
       std::cout << std::setw( 20 ) << secondary_reserve[ t ].get_value();
      std::cout << " ]" << std::endl;
     }
    }
    for( Index l = 0 ; l < hydro_block->get_number_generators() ; ++l ) {
     auto flow_rate = hydro_block->get_flow_rate( l );
     std::cout << "Flow rate    [" + std::to_string( l ) + "]" " = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
      std::cout << std::setw( 25 ) << std::setprecision( 14 )
                << flow_rate[ t ].get_value();
     std::cout << " ]" << std::endl;
    }

    for( Index n = 0 ; n < hydro_block->get_number_reservoirs() ; ++n ) {
     auto volumetric = hydro_block->get_volumetric( n );
     std::cout << "Volumetric   [" + std::to_string( n ) + "]" " = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
      std::cout << std::setw( 25 ) << std::setprecision( 14 )
                << volumetric[ t ].get_value();
     std::cout << " ]" << std::endl;
    }
    std::cout << std::setprecision( 8 );
   }

   if( auto hsu_block = dynamic_cast< HydroSystemUnitBlock * >( unit_block ) ) {

    for( Index hIdx = 0 ;
         hIdx < hsu_block->get_number_hydro_units() ; ++hIdx ) {
     std::cout << "----- SubHydroBlock " << hIdx << " -----" << std::endl;
     // for each hydro block inside, print the solution
     if( auto sub_hsu_block = hsu_block->
      get_hydro_unit_block( hIdx ) ) {
      for( Index g = 0 ; g < sub_hsu_block->get_number_generators() ; ++g ) {
       auto active_power = sub_hsu_block->get_active_power( g );
       std::cout << "Active power [" + std::to_string( g ) + "]" " = [";
       for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
        std::cout << std::setw( 20 ) << active_power[ t ].get_value();
       std::cout << " ]" << std::endl;
      }

      if( number_primary_zones > 0 ) {
       for( Index g = 0 ; g < sub_hsu_block->get_number_generators() ; ++g ) {
        auto primary_reserve = sub_hsu_block
         ->get_primary_spinning_reserve( g );
        std::cout << "Primary reserve [" + std::to_string( g ) + "]" " = [";
        for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
         std::cout << std::setw( 20 ) << primary_reserve[ t ].get_value();
        std::cout << " ]" << std::endl;
       }
      }
      if( number_secondary_zones > 0 ) {
       for( Index g = 0 ; g < sub_hsu_block->get_number_generators() ; ++g ) {
        auto secondary_reserve = sub_hsu_block
         ->get_secondary_spinning_reserve( g );
        std::cout << "Secondary reserve [" + std::to_string( g ) + "]" " = [";
        for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
         std::cout << std::setw( 20 ) << secondary_reserve[ t ].get_value();
        std::cout << " ]" << std::endl;
       }
      }

      for( Index l = 0 ; l < sub_hsu_block->get_number_generators() ; ++l ) {
       auto flow_rate = sub_hsu_block->get_flow_rate( l );
       std::cout << "Flow rate    [" + std::to_string( l ) + "]" " = [";
       for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
        std::cout << std::setw( 25 ) << std::setprecision( 14 )
                  << flow_rate[ t ].get_value();
       std::cout << " ]" << std::endl;
      }

      for( Index n = 0 ; n < sub_hsu_block->get_number_reservoirs() ; ++n ) {
       auto volumetric = sub_hsu_block->get_volumetric( n );
       std::cout << "Volumetric   [" + std::to_string( n ) + "]" " = [";
       for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
        std::cout << std::setw( 25 ) << std::setprecision( 14 )
                  << volumetric[ t ].get_value();
       std::cout << " ]" << std::endl;
      }
      std::cout << std::setprecision( 8 );
     }
    }
   }

   if( auto intermittent_unit_block =
    dynamic_cast< IntermittentUnitBlock * >( unit_block ) ) {

    if( intermittent_unit_block->get_investment_cost() != 0 )
     std::cout << "Capacity       = " <<
               intermittent_unit_block->get_design().get_value() *
               intermittent_unit_block->get_max_capacity() << std::endl;

    auto active_power = intermittent_unit_block->get_active_power( 0 );
    std::cout << "Active power   = [";
    for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
     std::cout << std::setw( 20 ) << active_power[ t ].get_value();
    std::cout << " ]" << std::endl;

    if( number_primary_zones > 0 ) {
     auto primary_spinning_reserve = intermittent_unit_block
      ->get_primary_spinning_reserve( 0 );
     std::cout << "Primary reserve     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
      std::cout << std::setw( 20 ) << primary_spinning_reserve[ t ].get_value();
     std::cout << " ]" << std::endl;
    }

    if( number_secondary_zones > 0 ) {
     auto secondary_spinning_reserve = intermittent_unit_block
      ->get_secondary_spinning_reserve( 0 );
     std::cout << "Secondary reserve     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
      std::cout << std::setw( 20 )
                << secondary_spinning_reserve[ t ].get_value();
     std::cout << " ]" << std::endl;
    }
   }

   if( auto slack_unit_block =
    dynamic_cast< SlackUnitBlock * >( unit_block ) ) {

    auto active_power = slack_unit_block->get_active_power( 0 );
    std::cout << "Active power   = [";
    for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
     std::cout << std::setw( 20 ) << active_power[ t ].get_value();
    std::cout << " ]" << std::endl;

    if( number_inertia_zones > 0 ) {
     auto commitment = slack_unit_block->get_commitment( 0 );
     std::cout << "Commitment     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
      std::cout << std::setw( 2 )
                << ( unsigned int ) round( commitment[ t ].get_value() );
     std::cout << " ]" << std::endl;
    }

    if( number_primary_zones > 0 ) {
     auto primary_spinning_reserve = slack_unit_block
      ->get_primary_spinning_reserve( 0 );
     std::cout << "Primary reserve     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
      std::cout << std::setw( 20 ) << primary_spinning_reserve[ t ].get_value();
     std::cout << " ]" << std::endl;
    }

    if( number_secondary_zones > 0 ) {
     auto secondary_spinning_reserve = slack_unit_block
      ->get_secondary_spinning_reserve( 0 );
     std::cout << "Secondary reserve     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t )
      std::cout << std::setw( 20 )
                << secondary_spinning_reserve[ t ].get_value();
     std::cout << " ]" << std::endl;
    }
   }

  } else if( auto network_block = dynamic_cast< NetworkBlock * >( i ) ) {

   std::cout << "----- " << network_block->classname() << " " <<
             n_net_blocks++ << " -----" << std::endl;

   if( auto obj =
    dynamic_cast< FRealObjective * >( network_block->get_objective() ) ) {
    auto fun = obj->get_function();
    fun->compute();
    std::cout << "Function value   = " << fun->get_value() << std::endl;
   }

   /* std::cout << "Node injection   = [" << std::endl;
   for( Index t = 0 ; t < network_block->get_number_intervals() ; ++t ) {
    auto node_inj = network_block->get_node_injection( t );
    for( Index j = 0 ; j < network_block->get_number_nodes() ; ++j )
     std::cout << std::setw( 20 ) << node_inj[ j ].get_value();
    std::cout << std::endl;
   }
   std::cout << " ]" << std::endl; */

   if( auto dc_network_block =
    dynamic_cast< DCNetworkBlock * >( network_block ) ) {

    auto power_flow = dc_network_block->get_power_flow();
    std::cout << "Power flow       = [";
    for( auto & n : power_flow )
     std::cout << std::setw( 20 ) << n.get_value();
    std::cout << " ]" << std::endl;

    auto auxiliary_var = dc_network_block->get_auxiliary_variable();
    if( ! auxiliary_var.empty() ) {
     std::cout << "Auxiliary variable     = [";
     for( auto & n : auxiliary_var )
      std::cout << std::setw( 20 ) << n.get_value();
     std::cout << " ]" << std::endl;
    }

   } else if( auto ec_network_block =
    dynamic_cast< ECNetworkBlock * >( network_block ) ) {

    auto shared_power = ec_network_block->get_shared_power();
    if( ! shared_power.empty() ) {
     std::cout << "Shared power     = [";
     for( auto & n : shared_power )
      std::cout << std::setw( 20 ) << n.get_value();
     std::cout << " ]" << std::endl;
    }

    std::cout << "Public power injection   = [" << std::endl;
    for( Index t = 0 ; t < ec_network_block->get_number_intervals() ; ++t ) {
     auto power_inj = ec_network_block->get_power_injection( t );
     for( Index j = 0 ; j < ec_network_block->get_number_nodes() ; ++j )
      std::cout << std::setw( 20 ) << power_inj[ j ].get_value();
     std::cout << std::endl;
    }
    std::cout << " ]" << std::endl;

    std::cout << "Public power absorption   = [" << std::endl;
    for( Index t = 0 ; t < ec_network_block->get_number_intervals() ; ++t ) {
     auto power_abs = ec_network_block->get_power_absorption( t );
     for( Index j = 0 ; j < ec_network_block->get_number_nodes() ; ++j )
      std::cout << std::setw( 20 ) << power_abs[ j ].get_value();
     std::cout << std::endl;
    }
    std::cout << " ]" << std::endl;

    auto peak_power = ec_network_block->get_peak_power();
    std::cout << "Peak power       = [";
    for( auto & n : peak_power )
     std::cout << std::setw( 20 ) << n.get_value();
    std::cout << " ]" << std::endl;
   }
  }
  std::cout << std::endl;
 }
}

/*--------------------------------------------------------------------------*/

/// Prints the content of a solved UCBlock
void print_UCBlock_solver_results( Block * block ,
                                   int solution_output_type ) {

 if( ! ( solution_output_type > 0 && solution_output_type < 4 ) )
  return;

 if( solution_output_type == 1 || solution_output_type == 3 )
  print_UCBlock_solver_results( block );

 if( solution_output_type == 2 || solution_output_type == 3 ) {
  auto solver = block->get_registered_solvers().front();
  solver->get_var_solution();

  if( auto cda_solver = dynamic_cast< CDASolver * >( solver ) ) {
   if( cda_solver->has_dual_solution() )
    cda_solver->get_dual_solution();
  }

  UCBlockSolutionOutput output;
  output.print( block );
 }
}
