#include <iostream>
#include <iomanip>
#include <fstream>
#include <getopt.h>

#include <UCBlock.h>
#include <ThermalUnitBlock.h>
#include <BusNetworkBlock.h>
#include <BatteryUnitBlock.h>
#include <HydroUnitBlock.h>

#include <IntermittentUnitBlock.h>


using namespace SMSpp_di_unipi_it;

std::string filename{};
std::string lp_file{};
std::string solver_name{};

void print_help() {
 // http://docopt.org
 std::cout << "Usage: ucblock_solver [options] <nc4-file>" << std::endl
           << std::endl
           << "Options:" << std::endl
           << "  -s <solver>, --solver <solver>  Choose solver." << std::endl
           << "                                  Available solvers are: cplex, dp." << std::endl
           << "  -w <file>, --writelp <file>     Write LP problem on file." << std::endl
           << "  -h, --help                      Print this help." << std::endl;
}

void process_args( int argc, char ** argv ) {

 if( argc < 2 ) {
  print_help();
  exit( 1 );
 }

 const char * const short_opts = "s:w:h";
 const option long_opts[] = {
  { "solver",  required_argument, nullptr, 's' },
  { "writelp", required_argument, nullptr, 'w' },
  { "help",    no_argument,       nullptr, 'h' },
  { nullptr,   no_argument,       nullptr, 0 }
 };

 // Options
 while( true ) {
  const auto opt = getopt_long( argc, argv, short_opts, long_opts, nullptr );

  if( -1 == opt ) {
   break;
  }

  switch( opt ) {
   case 's':
    solver_name = std::string( optarg );
   case 'w':
    lp_file = std::string( optarg );
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

 int type;
 gtype.getValues( &type );

 if( type != eBlockFile ) {
  std::cerr << filename << " is not an SMS++ nc4 Block file" << std::endl;
  exit( 1 );
 }

 netCDF::NcGroup bg = f.getGroup( "Block_0" );
 if( bg.isNull() ) {
  std::cerr << "Block_0 empty or undefined in " << filename << std::endl;
  exit( 1 );
 }

 // Deserialize block
 auto ucb = dynamic_cast<UCBlock *>(Block::new_Block( "UCBlock" ));
 ucb->deserialize( bg );

 // Configure blocks
 auto conf = new BlockConfig();
 for( auto i: ucb->get_nested_Blocks() ) {
  auto subconf = new BlockConfig();
  auto unit_block = dynamic_cast<UnitBlock *>(i);
  if( unit_block != nullptr ) {
   subconf->f_static_variables_Configuration = new SimpleConfiguration< int >( 15 );
  }
  conf->v_sub_BlockConfig.emplace_back( subconf );
 }

 // Configure solver
 auto slv_conf = new BlockSolverConfig();
 ComputeConfig comp_conf;

 if( solver_name == "cplex" ) {
  slv_conf->v_SolverNames.emplace_back( "CPXMILPSolver" );
  std::pair< std::string, std::string > problem_name = { "strProblemName",
                                                         "testCPX" };
  std::pair< std::string, double > accuracy = { "dblAAccSol", 1e-04 };
  std::pair< std::string, double > timelimit = { "dblMaxTime", 20000 };

  comp_conf.str_pars.emplace_back( problem_name );
  comp_conf.dbl_pars.emplace_back( accuracy );
  comp_conf.dbl_pars.emplace_back( timelimit );

  if( !lp_file.empty() ) {
   std::pair< std::string, std::string > output_file = { "strOutputFile",
                                                         lp_file };
   comp_conf.str_pars.emplace_back( output_file );
  }
  slv_conf->v_SolverConfigs.emplace_back( &comp_conf );

 } else if( solver_name == "dp" ) {
  std::cerr << "Sorry, DP Solver is not available yet..." << std::endl;
  exit( 0 );
 } else {
  std::cerr << "Available solvers are: cplex, dp" << std::endl;
  exit( 1 );
 }

 ucb->set_BlockConfig( conf );
 ucb->set_SolverConfig( slv_conf );
 std::cout.setf( std::ios::scientific, std::ios::floatfield );
 std::cout << std::setprecision( 8 );
 auto solver = ucb->get_registered_solvers().front();
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

    auto commitment = thermal_unit_block->get_commitment(0);
    std::cout << "Commitment     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 2 ) << ( unsigned int ) round( commitment[t].get_value());
    }
    std::cout << " ]" << std::endl;
    auto active_power = thermal_unit_block->get_active_power(0);
    std::cout << "active_power     = [";
    for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
     std::cout << std::setw( 20 ) <<   active_power[t].get_value();
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
   }

   auto battery_unit_block = dynamic_cast<BatteryUnitBlock *>(unit_block);
   if( battery_unit_block != nullptr ) {

    auto storage_level = battery_unit_block->get_storage_level();
    std::cout << "StorageLevel = [";
    for( auto & t : storage_level ) {
     std::cout << std::setw( 5 ) <<  t.get_value();
    }
    std::cout << " ]" << std::endl;

    auto Intake_level = battery_unit_block->get_intake_level();
    std::cout << "IntakeLevel  = [";
    for( auto & t : Intake_level ) {
     std::cout << std::setw( 5 ) << ( unsigned int ) round( t.get_value());
    }
    std::cout << " ]" << std::endl;


    auto Outtake_level = battery_unit_block->get_outtake_level();
    std::cout << "OuttakeLevel = [";
    for( auto & t : Outtake_level ) {
     std::cout << std::setw( 5 ) << ( unsigned int ) round( t.get_value());
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
     std::cout << "active_power     = [";
     for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
      std::cout << std::setw( 20 ) << active_power[t].get_value();
     }
     std::cout << " ]" << std::endl;
    }


    for( UnitBlock::Index l = 0; l < hydro_unit_block->get_number_generators(); ++l ) {
     auto flow_rate = hydro_unit_block->get_flow_rate( l );
      std::cout << "FlowRate   = [";
     for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
      std::cout << std::setw( 20 ) << flow_rate[t].get_value();
     }
     std::cout << " ]" << std::endl;
    }

    for( UnitBlock::Index n = 0; n < hydro_unit_block->get_number_reservoirs(); ++n ) {
     auto volumetric = hydro_unit_block->get_volumetric( n );
      std::cout << "Volumetric  = [";
     for( UnitBlock::Index t = 0; t < unit_block->get_time_horizon(); ++t ) {
      std::cout << std::setw( 20 ) << volumetric[t].get_value();
     }
     std::cout << " ]" << std::endl;
    }


   }
  }

  auto network_block = dynamic_cast<BusNetworkBlock *>(i);
  if( network_block != nullptr ) {
   std::cout << "----- NetworkBlock " << n_netw_blocks++ << std::endl;
   auto node_inj = network_block->get_node_injection();
   std::cout << "Node injection = "<< node_inj[ 0 ].get_value() << std::endl;
  }
  std::cout << std::endl;
 }

 return 0;
}
