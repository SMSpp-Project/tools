/*--------------------------------------------------------------------------*/
/*------------------------- File reopt_record.cpp --------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * The recording run of the computational study on the re-optimization of the
 * Min-Cost Flow problem. It loads a Multicommodity Min-Cost Flow (MMCF)
 * instance in a MMCFBlock, applies a BlockConfig and a BlockSolverConfig to
 * it, and solves it with the first Solver of the latter, which is meant to
 * be a LagrangianDualSolver whose subproblems, one MCFBlock per commodity,
 * are solved by MCFSolver<MCFTrace> [see config/LDBSCfg.txt]: MCFTrace then
 * writes to the trace each instance of the sequence it solves, which
 * reopt_bench -r gives to every method of the study. It prints one line of
 * comma-separated values:
 *
 *   instance,status,lb,ub,time,solves
 *
 * where lb and ub are the bounds the Solver gives (for a Lagrangian dual,
 * lb is its value and ub is infinite unless a feasible solution has been
 * found), time is that of compute() and solves the number of subproblems
 * solved, i.e., the length of the sequence summed over the commodities.
 *
 * Usage:
 *
 *   reopt_record [-c PATH] [-B FILE] [-S FILE] [-t TYPE] [-v] -o TRACE
 *                < mmcf-file >
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

#include <unistd.h>

#include <Block.h>
#include <BlockSolverConfig.h>
#include <CDASolver.h>

#include "MCFSolver.h"
#include "MMCFBlock.h"

#include "MCFTrace.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;
using namespace MCFClass_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*----------------------------- STATIC MEMBERS -----------------------------*/
/*--------------------------------------------------------------------------*/

SMSpp_insert_in_factory_cpp_0_t( MCFSolver< MCFTrace > );

// the parameters of MCFSolver< MCFTrace >: those of the CDASolver, plus
// kReopt, the parameters of MCFSimplex being left at their default

template<>
int MCFSolver< MCFTrace >::Solver_2_MCFClass_int( idx_type par ) const
{
 static const std::array< int , 7 > _val = {
  MCFClass::kMaxIter , -1 , -1 , -1 , -1 , -1 , MCFClass::kReopt };
 return( _val[ par ] );
 }

template<>
int MCFSolver< MCFTrace >::Solver_2_MCFClass_dbl( idx_type par ) const
{
 static const std::array< int , 12 > _val = {
  MCFClass::kMaxTime , -1 , -1 , MCFClass::kEpsFlw , -1 , -1 , -1 , -1 , -1 ,
  -1 , MCFClass::kEpsCst , -1 };
 return( _val[ par ] );
 }

template<>
Solver::idx_type MCFSolver< MCFTrace >::get_num_int_par( void ) const
{
 return( CDASolver::get_num_int_par() + 1 );
 }

template<>
int MCFSolver< MCFTrace >::get_dflt_int_par( idx_type par ) const
{
 return( par >= intLastParCDAS ? MCFClass::kYes
	                       : CDASolver::get_dflt_int_par( par ) );
 }

template<>
Solver::idx_type MCFSolver< MCFTrace >::int_par_str2idx(
					     const std::string & name ) const
{
 if( name == "kReopt" )
  return( intLastParCDAS );
 return( CDASolver::int_par_str2idx( name ) );
 }

template<>
const std::string & MCFSolver< MCFTrace >::int_par_idx2str( idx_type idx )
 const {
 static const std::string kreopt = "kReopt";
 return( idx >= intLastParCDAS ? kreopt : CDASolver::int_par_idx2str( idx ) );
 }

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static const char * const usage =
 "usage: reopt_record [options] -o TRACE < mmcf-file >\n"
 "  -c PATH  prefix of the configuration files [config/]\n"
 "  -B FILE  BlockConfig of the MMCFBlock [MMCFBCfg.txt]\n"
 "  -S FILE  BlockSolverConfig of the MMCFBlock [LDBSCfg.txt]\n"
 "  -t TYPE  format of the instance, as MMCFBlock::load() takes it [c]\n"
 "  -v       the log of the Solver on the standard error\n"
 "  -o FILE  the trace to write\n";

/*--------------------------------------------------------------------------*/
/*-------------------------------- main() ----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 std::string prefix = "config/";
 std::string bconf_file = "MMCFBCfg.txt";
 std::string sconf_file = "LDBSCfg.txt";
 std::string trace;
 char type = 'c';
 bool verbose = false;

 for( int opt ; ( opt = getopt( argc , argv , "c:B:S:t:vo:" ) ) != -1 ; )
  switch( opt ) {
   case 'c': prefix = optarg; break;
   case 'B': bconf_file = optarg; break;
   case 'S': sconf_file = optarg; break;
   case 't': type = optarg[ 0 ]; break;
   case 'v': verbose = true; break;
   case 'o': trace = optarg; break;
   default: std::cerr << usage; return( 1 );
   }

 if( ( optind != argc - 1 ) || trace.empty() ) {
  std::cerr << usage;
  return( 1 );
  }

 if( ( ! prefix.empty() ) && ( prefix.back() != '/' ) )
  prefix += '/';
 Configuration::set_filename_prefix( std::string( prefix ) );

 const std::string fn( argv[ optind ] );
 std::string instance = fn.substr( fn.find_last_of( '/' ) + 1 );
 instance = instance.substr( 0 , instance.find_last_of( '.' ) );

 // the MMCFBlock- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 auto MMCFB = new MMCFBlock;
 MMCFB->load( fn , type );
 MMCFB->PreProcess();

 auto bc = dynamic_cast< BlockConfig * >(
				    Configuration::deserialize( bconf_file ) );
 if( ! bc ) {
  std::cerr << "reopt_record: " << prefix + bconf_file
	    << " is not a BlockConfig" << std::endl;
  return( 1 );
  }
 bc->apply( MMCFB );
 delete bc;

 MMCFB->generate_abstract_variables();

 // the Solver - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 auto bsc = dynamic_cast< BlockSolverConfig * >(
				    Configuration::deserialize( sconf_file ) );
 if( ! bsc ) {
  std::cerr << "reopt_record: " << prefix + sconf_file
	    << " is not a BlockSolverConfig" << std::endl;
  return( 1 );
  }
 bsc->apply( MMCFB );
 bsc->clear();

 if( MMCFB->get_registered_solvers().empty() ) {
  std::cerr << "reopt_record: no Solver in " << sconf_file << std::endl;
  return( 1 );
  }
 Solver * solver = MMCFB->get_registered_solvers().front();
 if( verbose )
  solver->set_log( & std::cerr );

 // the solve, recorded- - - - - - - - - - - - - - - - - - - - - - - - - - -

 MCFTrace::open( trace );
 const auto start = std::chrono::steady_clock::now();
 const int status = solver->compute( false );
 const std::chrono::duration< double > t =
                                     std::chrono::steady_clock::now() - start;
 MCFTrace::close();

 std::cout << std::setprecision( 12 ) << instance << "," << status << ",";
 if( auto cda = dynamic_cast< CDASolver * >( solver ) )
  std::cout << cda->get_lb() << "," << cda->get_ub();
 else
  std::cout << ",";
 std::cout << "," << t.count() << "," << MCFTrace::solves() << std::endl;

 // clean up- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 bsc->apply( MMCFB );
 delete bsc;
 delete MMCFB;

 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*----------------------- End File reopt_record.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
