#include <iostream>
#include <iomanip>
#include <fstream>
#include <getopt.h>

#include <UCBlock.h>
#include <ThermalUnitBlock.h>
#include <BusNetworkBlock.h>
#include <DCNetworkBlock.h>
#include <BatteryUnitBlock.h>
#include <HydroUnitBlock.h>
#include <HydroSystemUnitBlock.h>

#include <IntermittentUnitBlock.h>
#include <CPXMILPSolver.h>


using namespace SMSpp_di_unipi_it;

std::string filename{};
std::string lp_file{};
std::string solver_name{};
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
           << "  -s <solver>, --solver <solver>  Choose solver." << std::endl
           << "                                  Available solvers are: cplex, dp." << std::endl
           << "  -w <file>, --writelp <file>     Write LP problem on file." << std::endl
           << "  -v, --verbose                   Make the solver verbose. " << std::endl
           << "  -h, --help                      Print this help." << std::endl;
}

void process_args( int argc, char ** argv ) {

 if( argc < 2 ) {
  print_help();
  exit( 1 );
 }

 const char * const short_opts = "B:S:s:w:vh";
 const option long_opts[] = {
  { "blockcfg",  required_argument, nullptr, 'b' },
  { "solvercfg", required_argument, nullptr, 's' },
  { "solver",    required_argument, nullptr, 's' },
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
   case 's':
    solver_name = std::string( optarg );
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

 solver_name = "cplex";
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
   auto b_config = new BlockConfig;
   std::ifstream bcf;
   bcf.open( bconf_file, std::ifstream::in );

   if( bcf ) {
    std::cout << "Using Block configuration in " << bconf_file << std::endl;
    try {
     bcf >> *b_config;
    } catch( const std::exception& e ) {
     std::cerr << "Block configuration not valid: " << e.what() << std::endl;
     exit( 1 );
    }
   } else {
    std::cout << "Block configuration not provided" << std::endl;

    // Default configuration
    for( auto i: ucb->get_nested_Blocks() ) {
     auto subconf = new BlockConfig();
     auto unit_block = dynamic_cast<UnitBlock *>(i);
     if( unit_block != nullptr ) {
      subconf->f_static_variables_Configuration = new SimpleConfiguration< int >( 15 );
     }

     auto hu_block = dynamic_cast<HydroSystemUnitBlock *>(i);
     if( hu_block != nullptr ) {
      auto subsubconf = new BlockConfig();
      for( auto j: i->get_nested_Blocks() ) {
       auto sub_pf_block = dynamic_cast<PolyhedralFunctionBlock *>(j);
       if( sub_pf_block != nullptr ) {
        subsubconf->f_static_variables_Configuration = new SimpleConfiguration< int >( 1 );
       }
       subconf->v_sub_BlockConfig.emplace_back( subsubconf );
      }
     }

     b_config->v_sub_BlockConfig.emplace_back( subconf );
    }
   }
   ucb->set_BlockConfig( b_config );

   // Configure solver
   std::cout << "Next in line : configure solver" << std::endl;
   std::cout.flush();

   auto s_config = new BlockSolverConfig;
   ;
   std::ifstream scf;
   scf.open( sconf_file, std::ifstream::in );

   if( scf ) {
    std::cout << "Using Solver configuration in " << sconf_file << std::endl;
    try {
     scf >> *s_config;
    } catch( ... ) {
     std::cout << "Solver configuration not valid" << std::endl;
     exit( 1 );
    }
   } else {
    std::cout << "Solver configuration not provided" << std::endl;

    ComputeConfig comp_conf;
    // Default configuration
    if( solver_name == "cplex" ) {
     s_config->v_SolverNames.emplace_back( "CPXMILPSolver" );
     // std::pair< std::string, std::string > problem_name = { "strProblemName",
     //                                                        "testCPX" };
     // std::pair< std::string, double > accuracy = { "dblAAccSol", 1e-04 };
     // std::pair< std::string, double > timelimit = { "dblMaxTime", 20000 };
     // std::pair< std::string, int > verbslvl = { "intLogVerb", 1 };

     // comp_conf.str_pars.emplace_back( problem_name );
     // comp_conf.dbl_pars.emplace_back( accuracy );
     // comp_conf.dbl_pars.emplace_back( timelimit );
     // if( solvVerbose == true ) {
     //  comp_conf.int_pars.emplace_back( verbslvl );
     // }

     // if( !lp_file.empty() ) {
     //  std::pair< std::string, std::string > output_file = { "strOutputFile",
     //                                                        lp_file };
     //  comp_conf.str_pars.emplace_back( output_file );
     // }
     s_config->v_SolverConfigs.emplace_back( &comp_conf );

    } else if( solver_name == "dp" ) {
     std::cerr << "Sorry, DP Solver is not available yet..." << std::endl;
     exit( 0 );
    } else {
     std::cerr << "Available solvers are: cplex, dp" << std::endl;
     exit( 1 );
    }
   }

   ucb->set_SolverConfig( s_config );

   break;
  }

  default:
   std::cerr << filename << " is not a valid SMS++ file" << std::endl;
   exit( 1 );
 }

 std::cout << "Data Loaded -- without foreseeable errors -- attempting to solve" << std::endl;
 std::cout.flush();
  
 std::cout.setf( std::ios::scientific, std::ios::floatfield );
 std::cout << std::setprecision( 8 );
 auto solver = ucb->get_registered_solvers().front();

 dynamic_cast<CPXMILPSolver*>(solver)->write_lp("test.lp");

 int status = solver->compute();
 solver->get_var_solution();
 auto ub = solver->get_ub();
 auto lb = solver->get_lb();
 std::cout << "Status = " << status << std::endl;
 std::cout << "Upper bound = " << ub << std::endl;
 std::cout << "Lower bound = " << lb << std::endl;

 int n_unit_blocks = 0;
 int n_netw_blocks = 0;

 std::cout << std::endl;

 for( auto i: ucb->get_nested_Blocks() ) {

  auto unit_block = dynamic_cast<UnitBlock *>(i);

  if( unit_block != nullptr ) {
   std::cout << "----- UnitBlock " << n_unit_blocks++ << std::endl;

   auto obj = dynamic_cast<FRealObjective*>(unit_block->get_objective());
   if (obj != nullptr) {
    auto fun = obj->get_function();
    fun->compute();
    std::cout << "Function value = " << fun->get_value() << std::endl;
   }

   auto thermal_unit_block = dynamic_cast<ThermalUnitBlock *>(unit_block);
   if( thermal_unit_block != nullptr ) {

   // auto Index= unit_block->get_nested_Block_index();

    auto ramp_up = thermal_unit_block->get_delta_ramp_up();
    auto ramp_down = thermal_unit_block->get_delta_ramp_down();
    auto init_up_down = thermal_unit_block->get_init_up_down_time();
    auto initial_power = thermal_unit_block->get_initial_power();
    auto min_power = thermal_unit_block->get_min_power();
    auto max_power = thermal_unit_block->get_max_power();


    if( ! ramp_up.empty() && ! ramp_down.empty() ) {

     if( init_up_down > 0 ) {
      for( UnitBlock:: Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
       if( initial_power + ramp_up[0] < min_power[0] ||
               initial_power - ramp_down[0] > max_power[0] ) {
        std::cout << "----- ThermalUnitBlock " << n_unit_blocks++ << std::endl;
        throw ( std::logic_error
        (         "::Ramp Constraints: when f_InitUpDownTime > 0, it must be "
                  "that f_initial_power + v_DeltaRampUp[ 0 ] >= v_MinPower[ 0 ]"
                  "f_initial_power - v_DeltaRampDown[ 0 ] <= v_MaxPower[ 0 ]" ));
       }
       }
     }
    }
    auto commitment = thermal_unit_block->get_commitment(0);
    std::cout << "Commitment     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 2 ) << ( unsigned int ) round( commitment[t].get_value());
    }
    std::cout << " ]" << std::endl;

    auto startup = thermal_unit_block->get_start_up();
    std::cout << "Start up     = [";
    for( auto & t : startup ) {
     std::cout << std::setw( 2 ) << ( unsigned int ) round( t.get_value());
    }
    std::cout << " ]" << std::endl;

    auto shutdown = thermal_unit_block->get_shut_down();
    std::cout << "Shut down    = [";
    for( auto & t : shutdown ) {
     std::cout << std::setw( 2 )
               << ( unsigned int ) round( t.get_value());
    }
    std::cout << " ]" << std::endl;

    auto active_power = thermal_unit_block->get_active_power(0);
    std::cout << "active_power     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) <<   active_power[t].get_value();
    }
    std::cout << " ]" << std::endl;
   }

   auto battery_unit_block = dynamic_cast<BatteryUnitBlock *>(unit_block);
   if( battery_unit_block != nullptr ) {


    auto active_power = battery_unit_block->get_active_power(0);
    std::cout << "active_power  = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) <<  active_power[t].get_value();
    }
    std::cout << " ]" << std::endl;


    auto PrimarySR = battery_unit_block->get_primary_spinning_reserve( 0 );

    std::cout << "PrimarySR     = [";

    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {

     std::cout << std::setw( 20 ) <<  PrimarySR[t].get_value();

    }

    std::cout << " ]" << std::endl;


    auto SecondarySR = battery_unit_block->get_secondary_spinning_reserve( 0 );

    std::cout << "SecondarySR     = [";

    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {

     std::cout << std::setw( 20 ) <<  SecondarySR[t].get_value();

    }

    std::cout << " ]" << std::endl;

    auto Intake_level = battery_unit_block->get_intake_level();
    std::cout << "IntakeLevel[:Storage]  = [";
    for( auto & t : Intake_level ) {
     std::cout << std::setw( 20 ) << ( unsigned int ) round( t.get_value());
    }
    std::cout << " ]" << std::endl;


    auto Outtake_level = battery_unit_block->get_outtake_level();
    std::cout << "OuttakeLevel[:Generation] = [";
    for( auto & t : Outtake_level ) {
     std::cout << std::setw( 20 ) << ( unsigned int ) round( t.get_value());
    }
    std::cout << " ]" << std::endl;

    auto storage_level = battery_unit_block->get_storage_level();
    std::cout << "StorageLevel = [";
    for( auto & t : storage_level ) {
     std::cout << std::setw( 20 ) <<  t.get_value();
    }
    std::cout << " ]" << std::endl;

    auto Binary_var = battery_unit_block->get_battery_binary();
    std::cout << "BinaryVar    = [";
    for( auto & t : Binary_var ) {
     std::cout << std::setw( 2 ) << ( unsigned int ) round( t.get_value());
    }
    std::cout << " ]" << std::endl;
   }


   auto hydro_unit_block = dynamic_cast<HydroUnitBlock *>(unit_block);
   if( hydro_unit_block != nullptr ) {
    for( UnitBlock::Index g = 0; g < unit_block->get_number_generators(); ++g ) {
     auto active_power = hydro_unit_block->get_active_power( g );
     std::cout << "active_power [" + std::to_string( g ) + "]" " = [";
     for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
      std::cout << std::setw( 20 ) << active_power[t].get_value();
     }
     std::cout << " ]" << std::endl;
    }


    for( UnitBlock::Index l = 0; l < hydro_unit_block->get_number_generators(); ++l ) {
     auto flow_rate = hydro_unit_block->get_flow_rate( l );
     std::cout << "FlowRate [" + std::to_string( l ) + "]" " = [";
     for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
      std::cout << std::setw( 25 ) << std::setprecision( 14 ) << flow_rate[t].get_value();
     }
     std::cout << " ]" << std::endl;
    }

    for( UnitBlock::Index n = 0; n < hydro_unit_block->get_number_reservoirs(); ++n ) {
     auto volumetric = hydro_unit_block->get_volumetric( n );
     std::cout << "Volumetric [" + std::to_string( n ) + "]" " = [";
     for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
      std::cout << std::setw( 25 ) << std::setprecision( 14 ) << volumetric[t].get_value();
     }
     std::cout << " ]" << std::endl;
    }
    std::cout << std::setprecision( 8 );

   }

   // If HydroSystemBlocks are there:
   auto hydro_sytem_block = dynamic_cast<HydroSystemUnitBlock *>(unit_block);
   if ( hydro_sytem_block != nullptr ) {
     for( UnitBlock::Index hIdx = 0; hIdx < hydro_sytem_block->get_number_hydro_units(); ++hIdx ) {
       std::cout << "----- SubHydroBlock " << hIdx << std::endl;
       // for each hydro block inside, print the solution
       auto hydro_unit_block = hydro_sytem_block->get_hydro_unit_block( hIdx ) ;  //dynamic_cast<HydroUnitBlock *>(unit_block);
       if (hydro_unit_block != nullptr) {
         for (UnitBlock::Index g = 0; g < hydro_unit_block->get_number_generators(); ++g) {
           auto active_power = hydro_unit_block->get_active_power(g);
          std::cout << "active_power [" + std::to_string( g ) + "]" " = [";
           for (UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t) {
             std::cout << std::setw(20) << active_power[t].get_value();
           }
           std::cout << " ]" << std::endl;
         }

         for (UnitBlock::Index l = 0; l < hydro_unit_block->get_number_generators(); ++l) {
           auto flow_rate = hydro_unit_block->get_flow_rate(l);
          std::cout << "FlowRate [" + std::to_string( l ) + "]" " = [";
           for (UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t) {
             std::cout << std::setw(25) << std::setprecision(14) << flow_rate[t].get_value();
           }
           std::cout << " ]" << std::endl;
         }

         for (UnitBlock::Index n = 0; n < hydro_unit_block->get_number_reservoirs(); ++n)  {
           auto volumetric = hydro_unit_block->get_volumetric(n);
          std::cout << "Volumetric [" + std::to_string( n ) + "]" " = [";
           for (UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t) {
             std::cout << std::setw(25) << std::setprecision(14) << volumetric[t].get_value();
           }
           std::cout << " ]" << std::endl;
         }
         std::cout << std::setprecision(8);
       }
     }
   }

   auto intermittent_unit_block = dynamic_cast<IntermittentUnitBlock *>(unit_block);
   if( intermittent_unit_block != nullptr ) {

    auto active_power = intermittent_unit_block->get_active_power(0);
    std::cout << "active_power     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) <<   active_power[t].get_value();
    }
    std::cout << " ]" << std::endl;

   }

  }


  auto network_block = dynamic_cast<NetworkBlock *>(i);
  if( network_block != nullptr ) {
   std::cout << "----- NetworkBlock " << n_netw_blocks++ << std::endl;


   auto node_inj = network_block->get_node_injection();
   std::cout << "Node injection     = [";
   for( int n = 0; n < node_inj.size(); ++n ) {
    std::cout << std::setw( 20 ) << node_inj[n].get_value();
   }
   std::cout << " ]" << std::endl;

   auto dc_network_block = dynamic_cast<DCNetworkBlock *>(network_block);

   if( dc_network_block != nullptr ) {
    auto power_flow = dc_network_block->get_power_flow();

    std::cout << "power_flow     = [";
   for( int n = 0; n < power_flow.size(); ++n ) {
    std::cout << std::setw( 20 ) << power_flow[n].get_value();
   }
   std::cout << " ]" << std::endl;
  }
  }


 }


 return 0;
}
