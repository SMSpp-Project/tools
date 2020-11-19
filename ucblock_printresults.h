/** @file
 * A procedure for printing the content of a solved UCBlock.
 *
 * \author Ali Ghezelsoflu \n
 *         Operations Research Group \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Niccolo' Iardella \n
 *         Operations Research Group \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * Copyright &copy; by Ali Ghezelsoflu, Niccolo' Iardella
 */

#include <BatteryUnitBlock.h>
#include <BusNetworkBlock.h>
#include <DCNetworkBlock.h>
#include <HydroSystemUnitBlock.h>
#include <HydroUnitBlock.h>
#include <SlackUnitBlock.h>
#include <IntermittentUnitBlock.h>
#include <ThermalUnitBlock.h>

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/

/// Prints the content of a solved UCBlock
void print_ucblock_solver_results( Block * block ) {
 int n_unit_blocks = 0;
 int n_netw_blocks = 0;

 std::cout << std::endl;

 for( auto i: block->get_nested_Blocks() ) {

  auto unit_block = dynamic_cast<UnitBlock *>(i);
  if( unit_block != nullptr ) {
   std::cout << "----- UnitBlock " << n_unit_blocks++ << std::endl;

   auto obj = dynamic_cast<FRealObjective *>(unit_block->get_objective());
   if( obj != nullptr ) {
    auto fun = obj->get_function();
    fun->compute();
    std::cout << "Function value = " << fun->get_value() << std::endl;
   }

   auto thermal_unit_block = dynamic_cast<ThermalUnitBlock *>(unit_block);
   if( thermal_unit_block != nullptr ) {

/*
    auto ramp_up = thermal_unit_block->get_delta_ramp_up();
    auto ramp_down = thermal_unit_block->get_delta_ramp_down();
    auto init_up_down = thermal_unit_block->get_init_up_down_time();
    auto initial_power = thermal_unit_block->get_initial_power();
    auto min_power = thermal_unit_block->get_min_power();
    auto max_power = thermal_unit_block->get_max_power();

    if( !ramp_up.empty() && !ramp_down.empty() ) {

     if( init_up_down > 0 ) {
      for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
       if( initial_power + ramp_up[ 0 ] < min_power[ 0 ] ||
           initial_power - ramp_down[ 0 ] > max_power[ 0 ] ) {
        std::cout << "----- ThermalUnitBlock " << n_unit_blocks++ << std::endl;
        throw ( std::logic_error
         ( "::Ramp Constraints: when f_InitUpDownTime > 0, it must be "
           "that f_initial_power + v_DeltaRampUp[ 0 ] >= v_MinPower[ 0 ]"
           "f_initial_power - v_DeltaRampDown[ 0 ] <= v_MaxPower[ 0 ]" ) );
       }
      }
     }
    }*/
    auto commitment = thermal_unit_block->get_commitment( 0 );
    std::cout << "Commitment     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 2 )
               << ( unsigned int ) round( commitment[ t ].get_value() );
    }
    std::cout << " ]" << std::endl;

    auto startup = thermal_unit_block->get_start_up();
    std::cout << "Start up     = [";
    for( auto & t : startup ) {
     std::cout << std::setw( 2 ) << ( unsigned int ) round( t.get_value() );
    }
    std::cout << " ]" << std::endl;

    auto shutdown = thermal_unit_block->get_shut_down();
    std::cout << "Shut down    = [";
    for( auto & t : shutdown ) {
     std::cout << std::setw( 2 )
               << ( unsigned int ) round( t.get_value() );
    }
    std::cout << " ]" << std::endl;

    auto active_power = thermal_unit_block->get_active_power( 0 );
    std::cout << "active_power     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) << active_power[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    auto primary_reserve = thermal_unit_block
     ->get_primary_spinning_reserve( 0 );
    std::cout << "primary_reserve     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) << primary_reserve[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    auto secondary_reserve = thermal_unit_block
     ->get_secondary_spinning_reserve( 0 );
    std::cout << "secondary_reserve     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) << secondary_reserve[ t ].get_value();
    }
    std::cout << " ]" << std::endl;
   }

   auto battery_unit_block = dynamic_cast<BatteryUnitBlock *>(unit_block);
   if( battery_unit_block != nullptr ) {

    auto active_power = battery_unit_block->get_active_power( 0 );
    std::cout << "active_power  = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) << active_power[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    auto PrimarySR = battery_unit_block->get_primary_spinning_reserve( 0 );
    std::cout << "primary_reserve     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) << PrimarySR[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    auto SecondarySR = battery_unit_block->get_secondary_spinning_reserve( 0 );
    std::cout << "secondary_reserve     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) << SecondarySR[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    auto Intake_level = battery_unit_block->get_intake_level();
    std::cout << "IntakeLevel[:Storage]  = [";
    for( auto & t : Intake_level ) {
     std::cout << std::setw( 20 ) << ( unsigned int ) round( t.get_value() );
    }
    std::cout << " ]" << std::endl;

    auto Outtake_level = battery_unit_block->get_outtake_level();
    std::cout << "OuttakeLevel[:Generation] = [";
    for( auto & t : Outtake_level ) {
     std::cout << std::setw( 20 ) << ( unsigned int ) round( t.get_value() );
    }
    std::cout << " ]" << std::endl;

    auto storage_level = battery_unit_block->get_storage_level();
    std::cout << "StorageLevel = [";
    for( auto & t : storage_level ) {
     std::cout << std::setw( 20 ) << t.get_value();
    }
    std::cout << " ]" << std::endl;

    auto Binary_var = battery_unit_block->get_battery_binary();
    std::cout << "BinaryVar    = [";
    for( auto & t : Binary_var ) {
     std::cout << std::setw( 2 ) << ( unsigned int ) round( t.get_value() );
    }
    std::cout << " ]" << std::endl;
   }

   auto hydro_unitblock = dynamic_cast<HydroUnitBlock *>(unit_block);
   if( hydro_unitblock != nullptr ) {
    for( UnitBlock::Index g = 0;
         g < unit_block->get_number_generators(); ++g ) {
     auto active_power = hydro_unitblock->get_active_power( g );
     std::cout << "active_power [" + std::to_string( g ) + "]" " = [";
     for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
      std::cout << std::setw( 20 ) << active_power[ t ].get_value();
     }
     std::cout << " ]" << std::endl;
    }

    for( UnitBlock::Index g = 0;
         g < unit_block->get_number_generators(); ++g ) {
     auto primary_reserve = hydro_unitblock->get_primary_spinning_reserve( g );
     std::cout << "primary_reserve [" + std::to_string( g ) + "]" " = [";
     for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
      std::cout << std::setw( 20 ) << primary_reserve[ t ].get_value();
     }
     std::cout << " ]" << std::endl;
    }

    for( UnitBlock::Index g = 0;
         g < unit_block->get_number_generators(); ++g ) {
     auto secondary_reserve = hydro_unitblock
      ->get_secondary_spinning_reserve( g );
     std::cout << "secondary_reserve [" + std::to_string( g ) + "]" " = [";
     for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
      std::cout << std::setw( 20 ) << secondary_reserve[ t ].get_value();
     }
     std::cout << " ]" << std::endl;
    }

    for( UnitBlock::Index l = 0;
         l < hydro_unitblock->get_number_generators(); ++l ) {
     auto flow_rate = hydro_unitblock->get_flow_rate( l );
     std::cout << "FlowRate [" + std::to_string( l ) + "]" " = [";
     for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
      std::cout << std::setw( 25 ) << std::setprecision( 14 )
                << flow_rate[ t ].get_value();
     }
     std::cout << " ]" << std::endl;
    }

    for( UnitBlock::Index n = 0;
         n < hydro_unitblock->get_number_reservoirs(); ++n ) {
     auto volumetric = hydro_unitblock->get_volumetric( n );
     std::cout << "Volumetric [" + std::to_string( n ) + "]" " = [";
     for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
      std::cout << std::setw( 25 ) << std::setprecision( 14 )
                << volumetric[ t ].get_value();
     }
     std::cout << " ]" << std::endl;
    }
    std::cout << std::setprecision( 8 );
   }

   // If HydroSystemBlocks are there:
   auto hydrosystem_unitblock = dynamic_cast<HydroSystemUnitBlock *>(unit_block);
   if( hydrosystem_unitblock != nullptr ) {
    for( UnitBlock::Index hIdx = 0;
         hIdx < hydrosystem_unitblock->get_number_hydro_units(); ++hIdx ) {
     std::cout << "----- SubHydroBlock " << hIdx << std::endl;
     // for each hydro block inside, print the solution
     auto sub_hydro_unitblock = hydrosystem_unitblock
      ->get_hydro_unit_block( hIdx );  //dynamic_cast<HydroUnitBlock *>(unit_block);
     if( sub_hydro_unitblock != nullptr ) {
      for( UnitBlock::Index g = 0;
           g < sub_hydro_unitblock->get_number_generators(); ++g ) {
       auto active_power = sub_hydro_unitblock->get_active_power( g );
       std::cout << "active_power [" + std::to_string( g ) + "]" " = [";
       for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
        std::cout << std::setw( 20 ) << active_power[ t ].get_value();
       }
       std::cout << " ]" << std::endl;
      }


      for( UnitBlock::Index g = 0;
           g < sub_hydro_unitblock->get_number_generators(); ++g ) {
       auto primary_reserve = sub_hydro_unitblock
        ->get_primary_spinning_reserve( g );
       std::cout << "primary_reserve [" + std::to_string( g ) + "]" " = [";
       for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
        std::cout << std::setw( 20 ) << primary_reserve[ t ].get_value();
       }
       std::cout << " ]" << std::endl;
      }

      for( UnitBlock::Index g = 0;
           g < sub_hydro_unitblock->get_number_generators(); ++g ) {
       auto secondary_reserve = sub_hydro_unitblock
        ->get_secondary_spinning_reserve( g );
       std::cout << "secondary_reserve [" + std::to_string( g ) + "]" " = [";
       for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
        std::cout << std::setw( 20 ) << secondary_reserve[ t ].get_value();
       }
       std::cout << " ]" << std::endl;
      }
      for( UnitBlock::Index l = 0;
           l < sub_hydro_unitblock->get_number_generators(); ++l ) {
       auto flow_rate = sub_hydro_unitblock->get_flow_rate( l );
       std::cout << "FlowRate [" + std::to_string( l ) + "]" " = [";
       for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
        std::cout << std::setw( 25 ) << std::setprecision( 14 )
                  << flow_rate[ t ].get_value();
       }
       std::cout << " ]" << std::endl;
      }

      for( UnitBlock::Index n = 0;
           n < sub_hydro_unitblock->get_number_reservoirs(); ++n ) {
       auto volumetric = sub_hydro_unitblock->get_volumetric( n );
       std::cout << "Volumetric [" + std::to_string( n ) + "]" " = [";
       for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
        std::cout << std::setw( 25 ) << std::setprecision( 14 )
                  << volumetric[ t ].get_value();
       }
       std::cout << " ]" << std::endl;
      }
      std::cout << std::setprecision( 8 );
     }
    }
   }

   auto intermittent_unit_block = dynamic_cast<IntermittentUnitBlock *>(unit_block);
   if( intermittent_unit_block != nullptr ) {

    auto active_power = intermittent_unit_block->get_active_power( 0 );
    std::cout << "active_power     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) << active_power[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    auto primary_spinning_reserve = intermittent_unit_block
     ->get_primary_spinning_reserve( 0 );
    std::cout << "primary_reserve     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) << primary_spinning_reserve[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    auto secondary_spinning_reserve = intermittent_unit_block
     ->get_secondary_spinning_reserve( 0 );
    std::cout << "secondary_reserve     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 )
               << secondary_spinning_reserve[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

   }

   auto slack_unit_block = dynamic_cast<SlackUnitBlock *>(unit_block);
   if( slack_unit_block != nullptr ) {

    auto active_power = slack_unit_block->get_active_power( 0 );
    std::cout << "active_power     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) << active_power[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    auto primary_spinning_reserve = slack_unit_block
     ->get_primary_spinning_reserve( 0 );
    std::cout << "primary_reserve     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) << primary_spinning_reserve[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    auto secondary_spinning_reserve = slack_unit_block
     ->get_secondary_spinning_reserve( 0 );
    std::cout << "secondary_reserve     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 )
               << secondary_spinning_reserve[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

   }
  }

  auto network_block = dynamic_cast<NetworkBlock *>(i);
  if( network_block != nullptr ) {
   std::cout << "----- NetworkBlock " << n_netw_blocks++ << std::endl;

   auto node_inj = network_block->get_node_injection();
   std::cout << "Node injection     = [";
   for( auto & n : node_inj ) {
    std::cout << std::setw( 20 ) << n.get_value();
   }
   std::cout << " ]" << std::endl;

   auto dc_network_block = dynamic_cast<DCNetworkBlock *>(network_block);

   if( dc_network_block != nullptr ) {
    auto power_flow = dc_network_block->get_power_flow();
    std::cout << "power_flow     = [";
    for( auto & n : power_flow ) {
     std::cout << std::setw( 20 ) << n.get_value();
    }
    std::cout << " ]" << std::endl;
   }
  }
 }
}
