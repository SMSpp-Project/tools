/*--------------------------------------------------------------------------*/
/*-------------------------- File ucblock_solver.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * SMS++ unit commitment solver.
 *
 * A tool that loads a UCBlock from an SMS++ nc4 Block file,
 * optionally configures it with a BlockConfig and a BlockSolverConfig,
 * and solves it with all the loaded Solvers.
 *
 * Optionally, it writes back the Block, the BlockConfig and the
 * BlockSolverConfig on an SMS++ nc4 problem file.
 *
 * \author Niccolo' Iardella \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Niccolo' Iardella, Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <iostream>
#include <exception>
#include <iomanip>

#include <Block.h>

#include "common_utils.h"
#include "UCBlock.h"

/*--------------------------------------------------------------------------*/
/* SAVE_TUB: if set to nonzero, an event is registered on the
 * LagrangianDualSolver (which forwards it to its inner [Parallel]BundleSolver)
 * that serializes each ThermalUnitBlock (the inner Block of each LagBFunction)
 * to its own SMS++ netCDF Block file "TUB-<unit>-<iteration>.nc4". The dump
 * frequency is the standard Solver::intEverykIt parameter of the inner Solver,
 * to be set in its ComputeConfig (see config/LDCfg.txt): the event fires every
 * k-th Lagrangian iteration, so dumps come naturally strided. Switch off
 * (default) by leaving SAVE_TUB undefined / 0; switch on by compiling with
 * -DSAVE_TUB=1. */

#ifndef SAVE_TUB
 #define SAVE_TUB 0
#endif

#if SAVE_TUB
 #include <memory>

 #include <Solver.h>
 #include <FRealObjective.h>
 #include <C05Function.h>
 #include <LagBFunction.h>

 #include "LagrangianDualSolver.h"
 #include "ThermalUnitBlock.h"
#endif

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 // ucblock_solver has no tool-specific options
 return( false );
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // append new options to default ones- - - - - - - - - - - - - - - - - - - -
 // note that the local options are inserted right before the last (nullptr)
 // record in long_opts

 docopt_desc =
  "SMS++ UCBlock solver: loads a Unit Commitment problem (a UCBlock) and\n"
  "solves it with the Solvers of its BlockSolverConfig.\n";
 docopt_args =
  "  <file>    SMS++ netCDF file (.nc4) holding a UCBlock: a Block file,\n"
  "            or a problem file of which only the Block is used;\n"
  "            <file>[i] selects the i-th Block (or problem) of the file\n";
 docopt_examples =
  "  ucblock_solver instance.nc4\n"
  "      solve with the default configuration\n"
  "  ucblock_solver -B '' instance.nc4\n"
  "      the same, with the formulation stored in instance.nc4\n"
  "  ucblock_solver -O sol.nc4 -C OSolCfg.txt instance.nc4\n"
  "      also save the Solution of the units and of the network in sol.nc4\n"
  "  ucblock_solver -c myconfig/ instance.nc4\n"
  "      use the Configuration files in myconfig/, e.g. a modified copy\n"
  "      of the installed ones\n";
 // the default -B is the "meta" BlockConfig InnerBCfg.txt, which chooses the
 // formulation of the units and of the network and linearizes the
 // PolyhedralFunctionBlock, without which a :MILPSolver cannot handle the
 // objective of the instances that have one
 default_bconf_name = "InnerBCfg.txt";

 /*short_opts.append( my_short_opts );
 long_opts.insert( std::prev( long_opts.end() ) ,
		   my_long_opts.begin() , my_long_opts.end() );
 help.append( my_help );*/

 // process command-line arguments- - - - - - - - - - - - - - - - - - - - - -

 process_args( argc , argv , process_specific_arg );

 // deserialize UCBlock - - - - - - - - - - - - - - - - - - - - - - - - - - -
 Block * block = get_Block( filename );

 if( ! dynamic_cast< UCBlock * >( block ) ) {
  std::cerr << exe << ": " << filename << " is not a UCBlock" << std::endl;
  exit( 1 );
  }

 // BlockConfig-ure and BlockSolverConfig-ure the [UC]Block - - - - - - - - -
 // only the BlockSolverConfig (-S) is mandatory; without a BlockConfig (-B,
 // the formulation, InnerBCfg.txt by default) the UCBlock is solved in the
 // formulation it is deserialized with
 require_solver_config( sconf_file );

 auto b_config = get_config( bconf_file );
 auto s_config = get_config( sconf_file );

 config_Block( block , b_config , s_config );

#if SAVE_TUB
 // register the ThermalUnitBlock-dumping event - - - - - - - - - - - - - - -
 // The Solver registered to the (UC)Block must be a LagrangianDualSolver
 // whose inner Solver is a [Parallel]BundleSolver: the latter is what
 // actually iterates on the Lagrangian dual, and the LagrangianDualSolver
 // transparently forwards both set_par() and set_event_handler() to it.
 {
  LagrangianDualSolver * lds = nullptr;
  for( auto * s : block->get_registered_solvers() )
   if( ( lds = dynamic_cast< LagrangianDualSolver * >( s ) ) )
    break;

  if( ! lds )
   std::cerr << exe << ": SAVE_TUB is on but no LagrangianDualSolver is "
                "registered to the Block; no ThermalUnitBlock will be saved"
             << std::endl;
  else if( const int k =
            lds->get_inner_Solver()->get_int_par( Solver::intEverykIt ) ;
           k < 1 )
   // dumping every iteration of every unit can be hundreds of thousands of
   // small files, so no default is silently applied here: the dump frequency
   // must be set explicitly in the inner Solver ComputeConfig
   std::cerr << exe << ": SAVE_TUB is on but intEverykIt is not set in the "
                "inner Solver ComputeConfig; no ThermalUnitBlock will be saved"
             << std::endl;
  else {
   // event counter, captured (by shared_ptr) into the handler; the event
   // first fires at iteration k and then every k iterations, so the true
   // iteration number put in the filename is (event count) * k
   auto iter = std::make_shared< int >( 0 );

   lds->set_event_handler( ThinComputeInterface::eEverykIteration ,
    [ lds , iter , k ]() -> int {
     const int it = ++( *iter ) * k;

     // the inner BundleSolver is registered to the Lagrangian-dual Block (LB)
     // built by the LagrangianDualSolver: each of its sub-Blocks (LB_i) holds
     // an FRealObjective whose Function is a LagBFunction, whose inner Block
     // is the original unit (B_i) -- here a ThermalUnitBlock
     Block * LB = lds->get_inner_Solver()->get_Block();
     if( ! LB )
      return( ThinComputeInterface::eContinue );

     Block::Index unit = 0;
     for( auto * sub : LB->get_nested_Blocks() ) {
      // sub-Block with an FRealObjective ...
      if( auto * fro =
              dynamic_cast< FRealObjective * >( sub->get_objective() ) ) {
       // ... containing a C05Function ...
       if( auto * c05 =
               dynamic_cast< C05Function * >( fro->get_function() ) )
        // ... which is a LagBFunction ...
        if( auto * lbf = dynamic_cast< LagBFunction * >( c05 ) )
         // ... whose inner Block is a ThermalUnitBlock
         if( auto * tub = dynamic_cast< ThermalUnitBlock * >(
                                            lbf->get_inner_block() ) ) {
          const std::string fname = "TUB-" + std::to_string( unit ) + "-" +
                                    std::to_string( it ) + ".nc4";
          // NB: Block::-qualified to bypass the serialize( NcGroup & )
          // override that would otherwise hide this base overload
          tub->Block::serialize( fname , eBlockFile );
          }
       }
      ++unit;
      }

     return( ThinComputeInterface::eContinue );
     } );

   std::cout << exe << ": SAVE_TUB on; dumping every ThermalUnitBlock every "
             << k << " iterations to TUB-<unit>-<iteration>.nc4" << std::endl;
   }
  }
#endif

 // write nc4 problem - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 if( writeprob )
  write_nc4problem( block , b_config , s_config );

 // solve - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 std::cout.setf( std::ios::scientific, std::ios::floatfield );
 std::cout << std::setprecision( 8 );

 set_solver_logs( block );

 solve_all( block );

 // cleanup - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 cleanup_bsc( block , s_config );
 delete s_config;
 delete block;

 return( 0 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*---------------------- End File ucblock_solver.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
