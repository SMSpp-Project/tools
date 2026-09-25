/*--------------------------------------------------------------------------*/
/*-------------------------- File ldld_bench.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * The driver of the computational study on the nested and the recursive
 * Lagrangian dual of a TwoStageStochasticBlock. It loads the first Block
 * of a Block file, applies a BlockConfig and a BlockSolverConfig to it,
 * keeps of the latter the one Solver chosen by -k, i.e., one method of the
 * study, and prints a single line of comma-separated values:
 *
 *   instance,method,status,lb,ub,time,iter,rss[,rub,rtime,gap]
 *
 * where time is that of compute() alone (the reading of the instance and
 * the construction of the Solver being excluded), iter the number of
 * iterations the Solver declares (for a LagrangianDualSolver, those of its
 * inner Solver, i.e., of the outer bundle), rss the peak resident memory of
 * the process in GB. With -R, a primal solution is recovered after the
 * dual: the here-and-now Variable are fixed to their mean over the leaves
 * (rounded where integer), each leaf is solved with the BlockSolverConfig
 * given, and rub is the sum of the leaf values, rtime the time of the
 * recovery and gap = ( rub - lb ) / | rub |. One process runs one method,
 * so that the memory of a method is not inflated by that of another.
 *
 * Usage:
 *
 *   ldld_bench [-c PATH] [-B FILE] [-S FILE] [-k N] [-m NAME] [-R FILE]
 *              [-j N] [-l FILE] [-b] < nc4-file >
 *
 * With -b, the Solver is attached to the Benders form of the
 * TwoStageStochasticBlock [see TwoStageStochasticBlock::get_Benders_form()],
 * i.e., to a Block whose Variable are the here-and-now ones and whose
 * sub-Block are the scenarios, as a BendersDecompositionSolver wants it; the
 * BlockSolverConfig is then one of that Block (e.g., BendersSCfg.txt).
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni, Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <thread>

#include <getopt.h>
#include <sys/resource.h>

#include <BlockSolverConfig.h>
#include <AbstractBlock.h>
#include <TwoStageStochasticBlock.h>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

using Index = Block::Index;

using MetaConfig = SimpleConfiguration< std::map< std::string ,
                                                  Configuration * > >;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

static std::string bconf_file = "InnerBCfg.txt";   // -B
static std::string sconf_file = "TSSBSCfg.txt";    // -S
static long which = -1;                            // -k, -1 = all
static std::string method;                         // -m
static std::string recover_sconf;                  // -R
static int recover_threads = 1;                    // -j
static std::string log_file;                       // -l
static bool benders = false;                       // -b

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static void usage( const char * exe )
{
 std::cerr << "usage: " << exe << " [-c PATH] [-B FILE] [-S FILE] [-k N]"
           << " [-m NAME] [-R FILE] [-j N] [-l FILE] [-b] <nc4-file>\n"
  "  -c PATH  prefix of the configuration files [config/]\n"
  "  -B FILE  [meta]BlockConfig of the Block tree [InnerBCfg.txt]\n"
  "  -S FILE  BlockSolverConfig of the root [TSSBSCfg.txt]\n"
  "  -k N     keep the N-th Solver of it alone [all, in turn]\n"
  "  -m NAME  name of the method in the output [the Solver classname]\n"
  "  -R FILE  recover a primal solution, the leaves being solved with the\n"
  "           BlockSolverConfig in FILE\n"
  "  -j N     threads solving the leaves in -R [1]\n"
  "  -l FILE  the log of the Solver goes to FILE [none]\n"
  "  -b       the Solver is attached to the Benders form of the Block\n";
 exit( 1 );
 }

/*--------------------------------------------------------------------------*/
// every Block of the tree rooted in block, breadth first

static std::list< Block * > all_Blocks( Block * block )
{
 std::list< Block * > BFS;
 BFS.push_back( block );
 for( auto it = BFS.begin() ; it != BFS.end() ; ++it )
  for( auto el : ( *it )->get_nested_Blocks() )
   BFS.push_back( el );
 return( BFS );
 }

/*--------------------------------------------------------------------------*/
// applies a [meta]BlockConfig to the tree rooted in block

static void apply_BlockConfig( Block * block , Configuration * cfg )
{
 if( ! cfg )
  return;

 if( auto meta = dynamic_cast< MetaConfig * >( cfg ) ) {
  for( auto b : all_Blocks( block ) ) {
   auto it = meta->f_value.find( b->classname() );
   if( it == meta->f_value.end() )
    it = meta->f_value.find( "*" );
   if( it != meta->f_value.end() )
    if( auto bc = dynamic_cast< BlockConfig * >( it->second ) ) {
     auto cbc = bc->clone();
     cbc->apply( b );
     delete cbc;
     }
   }
  return;
  }

 if( auto bc = dynamic_cast< BlockConfig * >( cfg ) ) {
  bc->apply( block );
  return;
  }

 throw( std::invalid_argument( "ldld_bench: " + bconf_file +
                               " is not a [meta]BlockConfig" ) );
 }

/*--------------------------------------------------------------------------*/
// peak resident memory of the process, in GB

static double peak_rss( void )
{
 struct rusage ru;
 getrusage( RUSAGE_SELF , &ru );
 return( double( ru.ru_maxrss ) / ( 1024.0 * 1024.0 ) );  // KB on Linux
 }

/*--------------------------------------------------------------------------*/
/* Recovers a primal solution of tssb: the here-and-now Variable of every
 * leaf are fixed to their mean over the leaves (rounded where integer),
 * which the Solver of the dual has written in them, and each leaf is then
 * solved on its own with a copy of the BlockSolverConfig in recover_sconf,
 * recover_threads leaves at a time. Returns the sum of the leaf values, or
 * +INF if a leaf is not solved. */

static double recover_primal( TwoStageStochasticBlock * tssb )
{
 const Index L = tssb->get_number_leaves();
 std::vector< std::vector< ColVariable * > > xk( L );
 for( Index l = 0 ; l < L ; ++l )
  for( const auto & p : tssb->get_paths_to_static_here_and_now_vars() ) {
   auto leaf = tssb->get_leaf_block( l );
   const auto nv = p->get_number_elements< ColVariable >( leaf );
   auto e = p->get_element< ColVariable >( leaf );
   for( Index j = 0 ; j < nv ; ++j )
    xk[ l ].push_back( e + j );
   }

 const Index n = L ? xk[ 0 ].size() : 0;
 std::vector< double > mean( n , 0 );
 for( Index l = 0 ; l < L ; ++l )
  for( Index j = 0 ; j < n ; ++j )
   mean[ j ] += xk[ l ][ j ]->get_value() / L;
 for( Index j = 0 ; j < n ; ++j )
  if( xk[ 0 ][ j ]->is_integer() )
   mean[ j ] = std::round( mean[ j ] );

 auto bsc = dynamic_cast< BlockSolverConfig * >(
                              Configuration::deserialize( recover_sconf ) );
 if( ! bsc )
  throw( std::invalid_argument( "ldld_bench: " + recover_sconf +
                                " is not a BlockSolverConfig" ) );

 std::vector< double > value( L , 0 );
 std::vector< char > solved( L , 0 );

 auto one = [ & ]( Index l ) {
  auto leaf = tssb->get_leaf_block( l );
  std::vector< bool > was_fixed( n );
  for( Index j = 0 ; j < n ; ++j ) {
   was_fixed[ j ] = xk[ l ][ j ]->is_fixed();
   xk[ l ][ j ]->set_value( mean[ j ] );
   xk[ l ][ j ]->is_fixed( true , eNoMod );
   }

  auto lbsc = bsc->clone();  // one per leaf, clear()-ing it detaches
  lbsc->apply( leaf );
  if( ! leaf->get_registered_solvers().empty() ) {
   auto solver = leaf->get_registered_solvers().front();
   const auto status = solver->compute();
   if( ( status == Solver::kOK ) || ( status == Solver::kLowPrecision ) ) {
    value[ l ] = solver->get_ub();
    solved[ l ] = 1;
    }
   }
  lbsc->clear();
  lbsc->apply( leaf );
  delete lbsc;

  for( Index j = 0 ; j < n ; ++j )
   if( ! was_fixed[ j ] )
    xk[ l ][ j ]->is_fixed( false , eNoMod );
  };

 const Index nt = std::min( Index( std::max( recover_threads , 1 ) ) , L );
 std::atomic< Index > next( 0 );
 std::exception_ptr error;
 std::mutex error_mutex;
 auto worker = [ & ]( void ) {
  for( Index l ; ( l = next++ ) < L ; )
   try {
    one( l );
    }
   catch( ... ) {
    std::lock_guard< std::mutex > guard( error_mutex );
    if( ! error )
     error = std::current_exception();
    next = L;
    return;
    }
  };

 std::vector< std::thread > pool;
 for( Index t = 1 ; t < nt ; ++t )
  pool.emplace_back( worker );
 worker();
 for( auto & th : pool )
  th.join();
 if( error )
  std::rethrow_exception( error );

 delete bsc;

 double ub = 0;
 for( Index l = 0 ; l < L ; ++l ) {
  if( ! solved[ l ] )
   return( Inf< double >() );
  ub += value[ l ];
  }
 return( ub );
 }

/*--------------------------------------------------------------------------*/
// runs the Solver of index k of the BlockSolverConfig and prints its line

static void run_one( const std::string & instance , const std::string & fn ,
                     Index k , Index ns )
{
 netCDF::NcFile file( fn , netCDF::NcFile::read );
 auto groups = file.getGroups();
 if( groups.empty() )
  throw( std::invalid_argument( "ldld_bench: " + fn + " holds no Block" ) );

 auto block = Block::new_Block( groups.begin()->second );
 if( ! block )
  throw( std::invalid_argument( "ldld_bench: " + fn +
                                " holds no valid Block" ) );

 auto bcfg = bconf_file.empty() ? nullptr
                                : Configuration::deserialize( bconf_file );
 apply_BlockConfig( block , bcfg );
 delete bcfg;

 auto bsc = dynamic_cast< BlockSolverConfig * >(
                                 Configuration::deserialize( sconf_file ) );
 if( ! bsc )
  throw( std::invalid_argument( "ldld_bench: " + sconf_file +
                                " is not a BlockSolverConfig" ) );

 // keep the k-th Solver alone: all the others are removed, last first
 for( Index i = ns ; i-- > 0 ; )
  if( i != k )
   bsc->remove_ComputeConfig( i );
 const std::string name = method.empty()
                          ? bsc->get_SolverName( 0 ) + "-" + std::to_string( k )
                          : method;

 // the Block the Solver is attached to: the TwoStageStochasticBlock, or its
 // Benders form, which is constructed out of its abstract representation
 auto tssb = dynamic_cast< TwoStageStochasticBlock * >( block );
 Block * target = block;
 if( benders ) {
  if( ! tssb )
   throw( std::invalid_argument( "ldld_bench: -b needs a "
                                 "TwoStageStochasticBlock" ) );
  tssb->generate_abstract_variables();
  tssb->generate_abstract_constraints();
  tssb->generate_objective();
  target = tssb->get_Benders_form();
  if( ! target )
   throw( std::invalid_argument( "ldld_bench: the Block has no "
                                 "here-and-now Variable, hence no Benders "
                                 "form" ) );
  }

 bsc->apply( target );
 bsc->clear();
 if( target->get_registered_solvers().empty() )
  throw( std::invalid_argument( "ldld_bench: no Solver attached" ) );
 auto solver = target->get_registered_solvers().front();

 std::ofstream log;
 if( ! log_file.empty() ) {
  log.open( log_file );
  solver->set_log( &log );
  }

 const auto start = std::chrono::steady_clock::now();
 const auto status = solver->compute();
 const std::chrono::duration< double > t =
                                     std::chrono::steady_clock::now() - start;

 const double lb = solver->get_lb();
 const double ub = solver->get_ub();

 std::cout << std::setprecision( 12 ) << instance << "," << name << ","
           << status << "," << lb << "," << ub << ","
           << std::setprecision( 6 ) << t.count() << ","
           << solver->get_elapsed_iterations() << "," << peak_rss();

 if( ( ! recover_sconf.empty() ) && ( ! benders ) ) {
  if( ! tssb )
   throw( std::invalid_argument( "ldld_bench: -R needs a "
                                 "TwoStageStochasticBlock" ) );
  if( solver->has_var_solution() )
   solver->get_var_solution();
  const auto rstart = std::chrono::steady_clock::now();
  const double rub = recover_primal( tssb );
  const std::chrono::duration< double > rt =
                                    std::chrono::steady_clock::now() - rstart;
  std::cout << std::setprecision( 12 ) << "," << rub << ","
            << std::setprecision( 6 ) << rt.count() << ","
            << ( rub - lb ) / std::abs( rub );
  }
 std::cout << std::endl;

 if( log.is_open() )
  solver->set_log( nullptr );

 bsc->apply( target );  // the clear()-ed BlockSolverConfig detaches
 delete bsc;
 if( target != block )
  tssb->give_back_Benders_form( static_cast< AbstractBlock * >( target ) );
 delete block;
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 std::string prefix = "config/";
 int opt;
 while( ( opt = getopt( argc , argv , "c:B:S:k:m:R:j:l:bh" ) ) != -1 )
  switch( opt ) {
   case 'c': prefix = optarg; break;
   case 'B': bconf_file = optarg; break;
   case 'S': sconf_file = optarg; break;
   case 'k': which = std::atol( optarg ); break;
   case 'm': method = optarg; break;
   case 'R': recover_sconf = optarg; break;
   case 'j': recover_threads = std::atoi( optarg ); break;
   case 'l': log_file = optarg; break;
   case 'b': benders = true; break;
   default: usage( argv[ 0 ] );
   }
 if( optind != argc - 1 )
  usage( argv[ 0 ] );

 if( ( ! prefix.empty() ) && ( prefix.back() != '/' ) )
  prefix += '/';
 Configuration::set_filename_prefix( std::string( prefix ) );

 const std::string fn = argv[ optind ];
 auto instance = fn.substr( fn.find_last_of( '/' ) + 1 );
 instance = instance.substr( 0 , instance.find_last_of( '.' ) );

 // the number of Solver in the BlockSolverConfig
 auto bsc = dynamic_cast< BlockSolverConfig * >(
                                 Configuration::deserialize( sconf_file ) );
 if( ! bsc ) {
  std::cerr << "ldld_bench: " << sconf_file << " is not a BlockSolverConfig"
            << std::endl;
  return( 1 );
  }
 const Index ns = bsc->get_SolverNames().size();
 delete bsc;
 if( ( which >= 0 ) && ( Index( which ) >= ns ) ) {
  std::cerr << "ldld_bench: -k " << which << " but " << sconf_file
            << " has " << ns << " Solver" << std::endl;
  return( 1 );
  }

 try {
  if( which >= 0 )
   run_one( instance , fn , which , ns );
  else
   for( Index k = 0 ; k < ns ; ++k )
    run_one( instance , fn , k , ns );
  }
 catch( std::exception & e ) {
  std::cerr << "ldld_bench: " << e.what() << std::endl;
  return( 1 );
  }

 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*------------------------ End File ldld_bench.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
