#include <iostream>
#include <iomanip>
#include <fstream>
#include <getopt.h>

#include <BatteryUnitBlock.h>
#include <BlockSolverConfig.h>
#include <BusNetworkBlock.h>
#include <DCNetworkBlock.h>
#include <HydroSystemUnitBlock.h>
#include <HydroUnitBlock.h>
#include <SlackUnitBlock.h>
#include <IntermittentUnitBlock.h>
#include <RBlockConfig.h>
#include <ThermalUnitBlock.h>
#include <UCBlock.h>

using namespace SMSpp_di_unipi_it;

std::string filename{};
std::string lp_file{};
std::string bconf_file{};
std::string sconf_file{};
bool solvVerbose = false;

void print_help() {
 // http://docopt.org
 std::cout << "Usage: ucblock_solver [options] <nc4-file>" << std::endl
           << std::endl
           << "Options:" << std::endl
           << "  -B <file>, --blockcfg <file>    Block configuration." << std::endl
           << "  -S <file>, --solvercfg <file>   Solver configuration." << std::endl
           << "  -w <file>, --writelp <file>     Write LP problem on file." << std::endl
           << "  -v, --verbose                   Make the solver verbose. " << std::endl
           << "  -h, --help                      Print this help." << std::endl;
}

void process_args( int argc, char ** argv ) {

 if( argc < 2 ) {
  print_help();
  exit( 1 );
 }

 const char * const short_opts = "B:S:w:vh";
 const option long_opts[] = {
  { "blockcfg",  required_argument, nullptr, 'B' },
  { "solvercfg", required_argument, nullptr, 'S' },
  { "writelp",   required_argument, nullptr, 'w' },
  { "verbose",   no_argument,       nullptr, 'v' },
  { "help",      no_argument,       nullptr, 'h' },
  { nullptr,     no_argument,       nullptr, 0 }
 };

 // Options
 while( true ) {
  const auto opt = getopt_long( argc, argv, short_opts, long_opts, nullptr );

  if( -1 == opt ) {
   break;
  }
  switch( opt ) {
   case 'B':
    bconf_file = std::string( optarg );
    break;
   case 'S':
    sconf_file = std::string( optarg );
    break;
   case 'w':
    lp_file = std::string( optarg );
    break;
   case 'v':
    solvVerbose = true;
    break;
   case 'h': // -h or --help
    print_help();
    exit( 0 );
   case '?': // Unrecognized option
   default:
    print_help();
    exit( 1 );
  }
 }

 // Last argument
 if( optind < argc ) {
  filename = std::string( argv[ optind ] );
 } else {
  print_help();
  exit( 1 );
 }
}

int main( int argc, char ** argv ) {

 process_args( argc, argv );

 netCDF::NcFile f;
 try {
  f.open( filename, netCDF::NcFile::read );
 } catch( netCDF::exceptions::NcException & e ) {
  std::cerr << "Cannot open nc4 file " << filename << std::endl;
  exit( 1 );
 }

 netCDF::NcGroupAtt gtype = f.getAtt( "SMS++_file_type" );
 if( gtype.isNull() ) {
  std::cerr << filename << " is not an SMS++ nc4 file" << std::endl;
  exit( 1 );
 }

 int type = 0;
 gtype.getValues( &type );
 auto ucb = dynamic_cast<UCBlock *>(Block::new_Block( "UCBlock" ));

 switch( type ) {
  case eProbFile: {
   std::cout << filename
             << " is a problem file, ignoring Block/Solver configurations..."
             << std::endl;
   // TODO
   break;
  }

  case eBlockFile: {
   std::cout << filename << " is a block file" << std::endl;

   netCDF::NcGroup bg = f.getGroup( "Block_0" );
   if( bg.isNull() ) {
    std::cerr << "Block_0 empty or undefined in " << filename << std::endl;
    exit( 1 );
   }

   std::cout << "Data Step -- Attempting to deserialize..." << std::endl;

   // Deserialize block
   ucb->deserialize( bg );

   // Configure block
   BlockConfig * b_config = nullptr;
   std::ifstream bcf;
   bcf.open( bconf_file, std::ifstream::in );
   if( bcf.is_open() ) {
    std::cout << "Using Block configuration in " << bconf_file << std::endl;
    std::string config_name;
    bcf >> config_name;
    b_config = dynamic_cast<BlockConfig *>
    ( Configuration::new_Configuration( config_name ) );
    if( !b_config ) {
     std::cerr << "Block configuration not valid: " << config_name << std::endl;
     exit( 1 );
    }
    try {
     bcf >> *b_config;
    } catch( const std::exception & e ) {
     std::cerr << "Block configuration not valid: " << e.what() << std::endl;
     exit( 1 );
    }
   } else {
    std::cout << "Block configuration not provided" << std::endl;

    // Default configuration
    b_config = new RBlockConfig;
    auto num_nested_Blocks_ucb = ucb->get_number_nested_Blocks();
    for( Block::Index i = 0; i < num_nested_Blocks_ucb; ++i ) {

     auto sub_Block_ucb = ucb->get_nested_Block( i );
     if( !dynamic_cast<UnitBlock *>( sub_Block_ucb ) )
      continue;

     auto subconf = new RBlockConfig;
     subconf->f_static_variables_Configuration =
      new SimpleConfiguration< int >( 15 );

     auto hu_block = dynamic_cast<HydroSystemUnitBlock *>( sub_Block_ucb );
     if( hu_block != nullptr ) {
      auto num_nested_blocks_hydro = hu_block->get_number_nested_Blocks();
      for( Block::Index j = 0; j < num_nested_blocks_hydro; ++j ) {
       auto sub_Block_hydro = hu_block->get_nested_Block( j );
       auto sub_pf_block =
        dynamic_cast<PolyhedralFunctionBlock *>( sub_Block_hydro );
       if( sub_pf_block != nullptr ) {
        auto subsubconf = new BlockConfig();
        subsubconf->f_static_variables_Configuration =
         new SimpleConfiguration< int >( 1 );
        subconf->add_sub_BlockConfig( subsubconf, j );
       }
      }
     }

     static_cast<RBlockConfig *>( b_config )->
      add_sub_BlockConfig( subconf, i );
    }
   }

   if( b_config )
    b_config->apply( ucb );

   // Configure solver
   std::cout << "Next in line : configure solver" << std::endl;
   std::cout.flush();

   BlockSolverConfig * s_config = nullptr;
   std::ifstream scf;
   scf.open( sconf_file, std::ifstream::in );

   if( scf.is_open() ) {
    std::cout << "Using Solver configuration in " << sconf_file << std::endl;
    std::string config_name;
    scf >> eatcomments >> config_name;
    s_config = dynamic_cast<BlockSolverConfig *>
    ( Configuration::new_Configuration( config_name ) );
    if( !s_config ) {
     std::cerr << "Solver configuration not valid: " << config_name
               << std::endl;
     exit( 1 );
    }
    try {
     scf >> *s_config;
    } catch( ... ) {
     std::cout << "Solver configuration not valid" << std::endl;
     exit( 1 );
    }
   } else {
    std::cout << "Solver configuration not provided" << std::endl;

    // Default configuration
    s_config = new BlockSolverConfig;
    auto comp_conf = new ComputeConfig;

    if( solvVerbose ) {
     std::pair< std::string, int > verbslvl = { "intLogVerb", 1 };
     comp_conf->int_pars.emplace_back( verbslvl );
    }
    if( !lp_file.empty() ) {
     std::pair< std::string, std::string > output_file = { "strOutputFile",
                                                           lp_file };
     comp_conf->str_pars.emplace_back( output_file );
    }

    s_config->add_ComputeConfig( "CPXMILPSolver", comp_conf );
   }

   if( s_config )
    s_config->apply( ucb );

   break;
  }

  default:
   std::cerr << filename << " is not a valid SMS++ file" << std::endl;
   exit( 1 );
 }

 std::cout << "Data Loaded -- without foreseeable errors -- attempting to solve"
           << std::endl;
 std::cout.flush();

 std::cout.setf( std::ios::scientific, std::ios::floatfield );
 std::cout << std::setprecision( 8 );
 auto solver = ucb->get_registered_solvers().front();

 int status = solver->compute();
 solver->get_var_solution();
 auto ub = solver->get_ub();
 auto lb = solver->get_lb();
 std::cout << "Status = " << status << " (";

 switch( status ) {
  case Solver::kOK:
   std::cout << "Success)" << std::endl;
   break;
  case Solver::kError:
   std::cout << "Error)" << std::endl;
   break;
  case Solver::kInfeasible:
   std::cout << "Infeasible)" << std::endl;
   break;
  case Solver::kUnbounded:
   std::cout << "Unbounded)" << std::endl;
   break;
  case Solver::kStopTime:
   std::cout << "Stopped for time limit)" << std::endl;
   break;
  case Solver::kStopIter:
   std::cout << "Stopped for iteration limit)" << std::endl;
   break;
  default:;
 }

 std::cout << "Upper bound = " << ub << std::endl;
 std::cout << "Lower bound = " << lb << std::endl;

 int n_unit_blocks = 0;
 int n_netw_blocks = 0;

 std::cout << std::endl;

 for( auto i: ucb->get_nested_Blocks() ) {

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

    auto primary_reserve = thermal_unit_block->get_primary_spinning_reserve( 0 );
    std::cout << "primary_reserve     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) << primary_reserve[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    auto secondary_reserve = thermal_unit_block->get_secondary_spinning_reserve( 0 );
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
    std::cout << "PrimarySR     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) << PrimarySR[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    auto SecondarySR = battery_unit_block->get_secondary_spinning_reserve( 0 );
    std::cout << "SecondarySR     = [";
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
     auto secondary_reserve = hydro_unitblock->get_secondary_spinning_reserve( g );
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
       auto primary_reserve = sub_hydro_unitblock->get_primary_spinning_reserve( g );
       std::cout << "primary_reserve [" + std::to_string( g ) + "]" " = [";
       for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
        std::cout << std::setw( 20 ) << primary_reserve[ t ].get_value();
       }
       std::cout << " ]" << std::endl;
      }

      for( UnitBlock::Index g = 0;
           g < sub_hydro_unitblock->get_number_generators(); ++g ) {
       auto secondary_reserve = sub_hydro_unitblock->get_secondary_spinning_reserve( g );
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

    auto primary_spinning_reserve = intermittent_unit_block->get_primary_spinning_reserve( 0 );
    std::cout << "PrimarySR     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) << primary_spinning_reserve[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    auto secondary_spinning_reserve = intermittent_unit_block->get_secondary_spinning_reserve( 0 );
    std::cout << "SecondarySR     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) << secondary_spinning_reserve[ t ].get_value();
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

    auto primary_spinning_reserve = slack_unit_block->get_primary_spinning_reserve( 0 );
    std::cout << "PrimarySR     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) << primary_spinning_reserve[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

    auto secondary_spinning_reserve = slack_unit_block->get_secondary_spinning_reserve( 0 );
    std::cout << "SecondarySR     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) << secondary_spinning_reserve[ t ].get_value();
    }
    std::cout << " ]" << std::endl;

   }
  }

  auto network_block = dynamic_cast<NetworkBlock *>(i);
  if( network_block != nullptr ) {
   std::cout << "----- NetworkBlock " << n_netw_blocks++ << std::endl;

   auto node_inj = network_block->get_node_injection();
   std::cout << "Node injection     = [";
   for(auto & n : node_inj) {
    std::cout << std::setw( 20 ) << n.get_value();
   }
   std::cout << " ]" << std::endl;

   auto dc_network_block = dynamic_cast<DCNetworkBlock *>(network_block);

   if( dc_network_block != nullptr ) {
    auto power_flow = dc_network_block->get_power_flow();
    std::cout << "power_flow     = [";
    for(auto & n : power_flow) {
     std::cout << std::setw( 20 ) << n.get_value();
    }
    std::cout << " ]" << std::endl;
   }
  }
 }
 return 0;
}
