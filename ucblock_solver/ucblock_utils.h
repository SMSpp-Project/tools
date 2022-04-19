/** @file
 * Utilities for the UC solver.
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
#include <CDASolver.h>
#include <DCNetworkBlock.h>
#include <HydroSystemUnitBlock.h>
#include <HydroUnitBlock.h>
#include <SlackUnitBlock.h>
#include <IntermittentUnitBlock.h>
#include <ThermalUnitBlock.h>

#include "UCBlockSolutionOutput.h"

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/

/// Returns a default UCBlock configuration
BlockConfig * default_configure_ucblock( Block * uc_block ) {

 auto b_config = new RBlockConfig;

 for( auto sb : uc_block->get_nested_Blocks() ) {
  if( !dynamic_cast<UnitBlock *>( sb ) ) {
   continue;
  }

  auto sbc = new RBlockConfig;

  // If HydroSystemUnitBlock, we configure its PolyhedralFunctionBlocks
  auto hu_block = dynamic_cast<HydroSystemUnitBlock *>( sb );

  if( hu_block != nullptr ) {
   auto num_nested_blocks_hydro = hu_block->get_number_nested_Blocks();
   for( auto ssb : hu_block->get_nested_Blocks() ) {

    auto pf_block = dynamic_cast<PolyhedralFunctionBlock *>( ssb );

    if( pf_block != nullptr ) {
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

 return b_config;
}

// BlockConfig * default_configure_ucblock( Block * uc_block ) {
//
//  BlockConfig * b_config = new RBlockConfig;
//  auto num_nested_blocks = uc_block->get_number_nested_Blocks();
//  for( Block::Index i = 0; i < num_nested_blocks; ++i ) {
//
//   auto sub_block = uc_block->get_nested_Block( i );
//   if( !dynamic_cast<UnitBlock *>( sub_block ) )
//    continue;
//
//   auto subconf = new RBlockConfig;
//   subconf->f_static_variables_Configuration =
//    new SimpleConfiguration< int >( 15 );
//
//   auto hu_block = dynamic_cast<HydroSystemUnitBlock *>( sub_block );
//
//   if( hu_block != nullptr ) {
//    auto num_nested_blocks_hydro = hu_block->get_number_nested_Blocks();
//    for( Block::Index j = 0; j < num_nested_blocks_hydro; ++j ) {
//     auto sub_Block_hydro = hu_block->get_nested_Block( j );
//     auto sub_pf_block =
//      dynamic_cast<PolyhedralFunctionBlock *>( sub_Block_hydro );
//
//     if( sub_pf_block != nullptr ) {
//      auto subsubconf = new BlockConfig();
//      subsubconf->f_static_variables_Configuration =
//       new SimpleConfiguration< int >( 1 );
//      subconf->add_sub_BlockConfig( subsubconf, j );
//     }
//    }
//   }
//
//   static_cast<RBlockConfig *>( b_config )->add_sub_BlockConfig( subconf, i );
//  }
//
//  return b_config;
// }

/*--------------------------------------------------------------------------*/
/*
void check_UCBlock_data( Block * block ) {
 auto uc_block = dynamic_cast<UCBlock *>(block);

 if( uc_block == nullptr )
  return;
 int n_network_blocks = 0;
 Index t = 0;
 for( Index n = 0 ; n < uc_block->get_number_networks() ; ++n ) {
  auto network = uc_block->get_network_block( n );
  for( Index i = 0 ; i < network->get_number_intervals() ; ++i , ++t ) {
   for( Index node = 0 ; node < network->get_number_nodes() ; ++node ) {
    double Active_power_demand;
    Active_power_demand = network->get_active_demand()[ node ];

    std::vector< double > v_Inflows;
    std::vector< double > v_MinVolumetric;
    std::vector< double > v_Water;
    double sum_max_power = 0;
    for( auto j : block->get_nested_Blocks() ) {

     auto thermal_unit_block = dynamic_cast<ThermalUnitBlock *>(j);
     if( thermal_unit_block ) {
      if( !thermal_unit_block->get_max_power().empty() ) {
       sum_max_power += thermal_unit_block->get_max_power()[ t ];
      }
      continue;
     }
     auto battery_unit_block = dynamic_cast<BatteryUnitBlock *>(j);
     if( battery_unit_block ) {
      if( !battery_unit_block->get_maximum_power().empty() ) {
       sum_max_power += battery_unit_block->get_maximum_power()[ t ];
      }
      continue;
     }
     auto slack_unit_block = dynamic_cast<SlackUnitBlock *>(j);
     if( slack_unit_block ) {
      if( !slack_unit_block->get_max_power().empty() ) {
       sum_max_power += slack_unit_block->get_max_power()[ t ];
      }
      continue;
     }
     int Kappa = 1;
     auto intermittent_unit_block = dynamic_cast<IntermittentUnitBlock *>(j);
     if( intermittent_unit_block ) {
      Kappa = intermittent_unit_block->get_kappa();
      if( !intermittent_unit_block->get_maximum_power().empty() ) {
       sum_max_power += ( intermittent_unit_block->get_maximum_power()[ t ] ) *
                        ( Kappa );
      }
      continue;
     }

     auto hydro_unit_block = dynamic_cast<HydroUnitBlock *>(j);
     if( hydro_unit_block ) {
      if( !hydro_unit_block->get_maximum_flow().empty() &&
          !hydro_unit_block->get_maximum_flow().empty() ) {
       if( !hydro_unit_block->get_maximum_power().empty() ) {
        for( Index g = 0 ;
             g < hydro_unit_block->get_number_generators() ; ++g ) {
         if( hydro_unit_block->get_maximum_flow()[ t ][ g ] > 0 &&
             hydro_unit_block->get_maximum_flow()[ t ][ g ] >= 0 ) { //TURBINE
          sum_max_power += hydro_unit_block->get_maximum_power()[ t ][ g ];
         } else if( hydro_unit_block->get_maximum_flow()[ t ][ g ] <= 0 &&
                    hydro_unit_block->get_maximum_flow()[ t ][ g ] <
                    0 ) {//PUMP TODO

          v_Inflows.resize( hydro_unit_block->get_number_reservoirs() );
          v_MinVolumetric.resize( hydro_unit_block->get_number_reservoirs() );
          v_Water.resize( hydro_unit_block->get_number_reservoirs() );

          for( Index g = 0 ;
               g < hydro_unit_block->get_number_reservoirs() ; ++g ) {

           auto InitialVolumetric = hydro_unit_block->get_initial_volumetric()[ g ];

           for( Index time = 0 ;
                time < uc_block->get_time_horizon() ; ++time ) {

            if( !hydro_unit_block->get_inflows().empty() ) {
             v_Inflows[ g ] += hydro_unit_block->get_inflows()[ g ][ time ];
            }
            if( !hydro_unit_block->get_minimum_volumetric().empty() ) {
            }
           }
           v_MinVolumetric[ g ] = hydro_unit_block->get_minimum_volumetric()[ g ][
            uc_block->get_time_horizon() - 1 ];
           v_Water[ g ] =
            ( v_Inflows[ g ] + InitialVolumetric ) - v_MinVolumetric[ g ];
          }
         }
        }
       }
      }
      continue;
     }
     auto hydrosystem_unitblock = dynamic_cast<HydroSystemUnitBlock *>(j);
     if( hydrosystem_unitblock != nullptr ) {
      for( Index hIdx = 0 ;
           hIdx < hydrosystem_unitblock->get_number_hydro_units() ; ++hIdx ) {
       auto sub_hydro_unitblock = hydrosystem_unitblock
        ->get_hydro_unit_block( hIdx );
       if( sub_hydro_unitblock != nullptr ) {
        if( !sub_hydro_unitblock->get_maximum_flow().empty() &&
            !sub_hydro_unitblock->get_maximum_flow().empty() ) {
         if( !sub_hydro_unitblock->get_maximum_power().empty() ) {
          for( Index g = 0 ;
               g < sub_hydro_unitblock->get_number_generators() ; ++g ) {
           if( sub_hydro_unitblock->get_maximum_flow()[ t ][ g ] > 0 &&
               sub_hydro_unitblock->get_maximum_flow()[ t ][ g ] >=
               0 ) { // TURBINE
            sum_max_power += sub_hydro_unitblock->get_maximum_power()[ t ][ g ];
           } else if( sub_hydro_unitblock->get_maximum_flow()[ t ][ g ] <= 0 &&
                      sub_hydro_unitblock->get_maximum_flow()[ t ][ g ] <
                      0 ) { //PUMP
            // TODO TODO
           }
          }
         }
        }
       }
      }
     }
    }
    if( sum_max_power < Active_power_demand ) {
     std::cout << "----- ActivePowerDemand " << n_network_blocks++ << std::endl;

     throw ( std::logic_error
      ( "::UCBlock_Data_Check: Available Power does not exceed the "
        "Load " ) );
    }
   }
  }
 }
} */

/// Prints the content of a solved UCBlock
void print_ucblock_solver_results( Block * block ) {

 auto solver = block->get_registered_solvers().front();
 solver->get_var_solution();

 int n_unit_blocks = 0;
 int n_netw_blocks = 0;

 std::cout << std::endl;

 for( auto i : block->get_nested_Blocks() ) {

  auto uc_block = dynamic_cast<UCBlock *>(block);

  Index number_primary_zones = uc_block->get_number_primary_zones();
  Index number_secondary_zones = uc_block->get_number_secondary_zones();
  Index number_inertia_zones = uc_block->get_number_inertia_zones();

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

    auto commitment = thermal_unit_block->get_commitment( 0 );
    std::cout << "Commitment     = [";
    for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
     std::cout << std::setw( 2 )
               << ( unsigned int ) round( commitment[ t ].get_value() );
    }
    std::cout << " ]" << std::endl;

    // Generate init_t
    int init_t;
    auto init_up_down_time = thermal_unit_block->get_init_up_down_time();
    auto min_up_time = thermal_unit_block->get_min_up_time();
    auto min_down_time = thermal_unit_block->get_min_down_time();
    if( init_up_down_time > 0 ) {
     init_t = init_up_down_time >= min_up_time ?
              0 : ( int ) min_up_time - init_up_down_time;
    } else {
     init_t = -init_up_down_time >= min_down_time ?
              0 : ( int ) min_down_time + init_up_down_time;
    }

    auto startup = thermal_unit_block->get_start_up();
    std::cout << "Start up     = [";
    for( Index t = 0 ; t < unit_block->get_time_horizon() - init_t ; ++t ) {
     std::cout << std::setw( 2 )
               << ( unsigned int ) round( startup[ t ].get_value() );
    }
    std::cout << " ]" << std::endl;

    auto shutdown = thermal_unit_block->get_shut_down();
    std::cout << "Shut down    = [";
    for( Index t = 0 ; t < unit_block->get_time_horizon() - init_t ; ++t ) {
     std::cout << std::setw( 2 )
               << ( unsigned int ) round( shutdown[ t ].get_value() );
    }
    std::cout << " ]" << std::endl;

    auto active_power = thermal_unit_block->get_active_power( 0 );
    std::cout << "active_power     = [";
    for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
     std::cout << std::setw( 20 ) << active_power[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    if( number_primary_zones > 0 ) {
     auto primary_reserve = thermal_unit_block
      ->get_primary_spinning_reserve( 0 );
     std::cout << "primary_reserve     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
      std::cout << std::setw( 20 ) << primary_reserve[ t ].get_value();
     }
     std::cout << " ]" << std::endl;
    }

    if( number_secondary_zones > 0 ) {
     auto secondary_reserve = thermal_unit_block
      ->get_secondary_spinning_reserve( 0 );
     std::cout << "secondary_reserve     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
      std::cout << std::setw( 20 ) << secondary_reserve[ t ].get_value();
     }
     std::cout << " ]" << std::endl;
    }
   }
   auto battery_unit_block = dynamic_cast<BatteryUnitBlock *>(unit_block);
   if( battery_unit_block != nullptr ) {

    auto active_power = battery_unit_block->get_active_power( 0 );
    std::cout << "active_power  = [";
    for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
     std::cout << std::setw( 20 ) << active_power[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    if( number_primary_zones > 0 ) {
     auto PrimarySR = battery_unit_block->get_primary_spinning_reserve( 0 );
     std::cout << "primary_reserve     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
      std::cout << std::setw( 20 ) << PrimarySR[ t ].get_value();
     }
     std::cout << " ]" << std::endl;
    }
    if( number_secondary_zones > 0 ) {
     auto SecondarySR = battery_unit_block->get_secondary_spinning_reserve( 0 );
     std::cout << "secondary_reserve     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
      std::cout << std::setw( 20 ) << SecondarySR[ t ].get_value();
     }
     std::cout << " ]" << std::endl;
    }
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
    for( Index g = 0 ;
         g < unit_block->get_number_generators() ; ++g ) {
     auto active_power = hydro_unitblock->get_active_power( g );
     std::cout << "active_power [" + std::to_string( g ) + "]" " = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
      std::cout << std::setw( 20 ) << active_power[ t ].get_value();
     }
     std::cout << " ]" << std::endl;
    }

    if( number_primary_zones > 0 ) {
     for( Index g = 0 ;
          g < unit_block->get_number_generators() ; ++g ) {
      auto primary_reserve = hydro_unitblock->get_primary_spinning_reserve( g );
      std::cout << "primary_reserve [" + std::to_string( g ) + "]" " = [";
      for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
       std::cout << std::setw( 20 ) << primary_reserve[ t ].get_value();
      }
      std::cout << " ]" << std::endl;
     }
    }
    if( number_secondary_zones > 0 ) {
     for( Index g = 0 ;
          g < unit_block->get_number_generators() ; ++g ) {
      auto secondary_reserve = hydro_unitblock
       ->get_secondary_spinning_reserve( g );
      std::cout << "secondary_reserve [" + std::to_string( g ) + "]" " = [";
      for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
       std::cout << std::setw( 20 ) << secondary_reserve[ t ].get_value();
      }
      std::cout << " ]" << std::endl;
     }
    }
    for( Index l = 0 ;
         l < hydro_unitblock->get_number_generators() ; ++l ) {
     auto flow_rate = hydro_unitblock->get_flow_rate( l );
     std::cout << "FlowRate [" + std::to_string( l ) + "]" " = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
      std::cout << std::setw( 25 ) << std::setprecision( 14 )
                << flow_rate[ t ].get_value();
     }
     std::cout << " ]" << std::endl;
    }

    for( Index n = 0 ;
         n < hydro_unitblock->get_number_reservoirs() ; ++n ) {
     auto volumetric = hydro_unitblock->get_volumetric( n );
     std::cout << "Volumetric [" + std::to_string( n ) + "]" " = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
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
    for( Index hIdx = 0 ;
         hIdx < hydrosystem_unitblock->get_number_hydro_units() ; ++hIdx ) {
     std::cout << "----- SubHydroBlock " << hIdx << std::endl;
     // for each hydro block inside, print the solution
     auto sub_hydro_unitblock = hydrosystem_unitblock
      ->get_hydro_unit_block(
       hIdx );  //dynamic_cast<HydroUnitBlock *>(unit_block);
     if( sub_hydro_unitblock != nullptr ) {
      for( Index g = 0 ;
           g < sub_hydro_unitblock->get_number_generators() ; ++g ) {
       auto active_power = sub_hydro_unitblock->get_active_power( g );
       std::cout << "active_power [" + std::to_string( g ) + "]" " = [";
       for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
        std::cout << std::setw( 20 ) << active_power[ t ].get_value();
       }
       std::cout << " ]" << std::endl;
      }

      if( number_primary_zones > 0 ) {
       for( Index g = 0 ;
            g < sub_hydro_unitblock->get_number_generators() ; ++g ) {
        auto primary_reserve = sub_hydro_unitblock
         ->get_primary_spinning_reserve( g );
        std::cout << "primary_reserve [" + std::to_string( g ) + "]" " = [";
        for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
         std::cout << std::setw( 20 ) << primary_reserve[ t ].get_value();
        }
        std::cout << " ]" << std::endl;
       }
      }
      if( number_secondary_zones > 0 ) {
       for( Index g = 0 ;
            g < sub_hydro_unitblock->get_number_generators() ; ++g ) {
        auto secondary_reserve = sub_hydro_unitblock
         ->get_secondary_spinning_reserve( g );
        std::cout << "secondary_reserve [" + std::to_string( g ) + "]" " = [";
        for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
         std::cout << std::setw( 20 ) << secondary_reserve[ t ].get_value();
        }
        std::cout << " ]" << std::endl;
       }
      }

      for( Index l = 0 ;
           l < sub_hydro_unitblock->get_number_generators() ; ++l ) {
       auto flow_rate = sub_hydro_unitblock->get_flow_rate( l );
       std::cout << "FlowRate [" + std::to_string( l ) + "]" " = [";
       for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
        std::cout << std::setw( 25 ) << std::setprecision( 14 )
                  << flow_rate[ t ].get_value();
       }
       std::cout << " ]" << std::endl;
      }

      for( Index n = 0 ;
           n < sub_hydro_unitblock->get_number_reservoirs() ; ++n ) {
       auto volumetric = sub_hydro_unitblock->get_volumetric( n );
       std::cout << "Volumetric [" + std::to_string( n ) + "]" " = [";
       for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
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
    for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
     std::cout << std::setw( 20 ) << active_power[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    if( number_primary_zones > 0 ) {
     auto primary_spinning_reserve = intermittent_unit_block
      ->get_primary_spinning_reserve( 0 );
     std::cout << "primary_reserve     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
      std::cout << std::setw( 20 ) << primary_spinning_reserve[ t ].get_value();
     }
     std::cout << " ]" << std::endl;
    }

    if( number_secondary_zones > 0 ) {
     auto secondary_spinning_reserve = intermittent_unit_block
      ->get_secondary_spinning_reserve( 0 );
     std::cout << "secondary_reserve     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
      std::cout << std::setw( 20 )
                << secondary_spinning_reserve[ t ].get_value();
     }
     std::cout << " ]" << std::endl;

    }
   }
   auto slack_unit_block = dynamic_cast<SlackUnitBlock *>(unit_block);
   if( slack_unit_block != nullptr ) {

    auto active_power = slack_unit_block->get_active_power( 0 );
    std::cout << "active_power     = [";
    for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
     std::cout << std::setw( 20 ) << active_power[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    if( number_inertia_zones > 0 ) {
     auto commitment = slack_unit_block->get_commitment( 0 );
     std::cout << "Commitment     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
      std::cout << std::setw( 2 )
                << ( unsigned int ) round( commitment[ t ].get_value() );
     }
     std::cout << " ]" << std::endl;
    }

    if( number_primary_zones > 0 ) {
     auto primary_spinning_reserve = slack_unit_block
      ->get_primary_spinning_reserve( 0 );
     std::cout << "primary_reserve     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
      std::cout << std::setw( 20 ) << primary_spinning_reserve[ t ].get_value();
     }
     std::cout << " ]" << std::endl;
    }

    if( number_secondary_zones > 0 ) {
     auto secondary_spinning_reserve = slack_unit_block
      ->get_secondary_spinning_reserve( 0 );
     std::cout << "secondary_reserve     = [";
     for( Index t = 0 ; t < unit_block->get_time_horizon() ; ++t ) {
      std::cout << std::setw( 20 )
                << secondary_spinning_reserve[ t ].get_value();
     }
     std::cout << " ]" << std::endl;
    }
   }
  }

  auto network_block = dynamic_cast<NetworkBlock *>(i);
  if( network_block != nullptr ) {
   std::cout << "----- NetworkBlock " << n_netw_blocks++ << std::endl;

   auto node_inj = network_block->get_node_injection();
   std::cout << "Node injection     = [";
   for( Index j = 0 ; j < network_block->get_number_nodes() ; ++j ) {
    std::cout << std::setw( 20 ) << node_inj[ j ].get_value();
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

    auto auxiliary_variable = dc_network_block->get_auxiliary_variable();
    std::cout << "auxiliary_variable     = [";
    for( auto & n : auxiliary_variable ) {
     std::cout << std::setw( 20 ) << n.get_value();
    }
    std::cout << " ]" << std::endl;

   }
  }
 }
}

/*--------------------------------------------------------------------------*/

/// Prints the content of a solved UCBlock
void print_ucblock_solver_results( UCBlock * block ,
                                   int solution_output_type ) {

 if( !( solution_output_type > 0 && solution_output_type < 4 ) )
  return;

 if( solution_output_type == 1 || solution_output_type == 3 )
  print_ucblock_solver_results( block );

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
