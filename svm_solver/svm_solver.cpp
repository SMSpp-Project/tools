/*--------------------------------------------------------------------------*/
/*-------------------------- File svm_solver.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * This is a convenient tool for training a Support Vector Machine, i.e., for
 * solving a SVMBlock, and for the model selection that surrounds it.
 *
 * The description of the SVMBlock must be given in a netCDF file, either a
 * BlockFile or a ProbFile, or in one of the two plain text formats that
 * SVMBlock reads, the dense one and, with -l, the sparse one of the LIBSVM
 * data sets [see SVMBlock::load( std::istream , char )], in which case -t
 * says whether it is a classification or a regression problem. This tool can be executed as
 * follows:
 *
 *   ./svm_solver [-B FILE] [-S FILE] [-O FILE] [-k NUMBER] [-x FRACTION]
 *                [-g SPEC] [-t TASK] [-e NUMBER] [-p PATH] [-c PATH]
 *                < file >
 *
 * With none of -k, -x and -g the SVMBlock is simply trained on all of its
 * samples and the value of the training problem is reported, in the format
 * any other SMS++ tool uses, and -O writes the trained model.
 *
 * The other three options are the model selection proper, i.e., the part that
 * is not an optimization problem and that any honest use of a model needs:
 *
 * - -x holds out the given fraction of the samples, trains on the rest and
 *   reports the score on the held-out ones;
 *
 * - -k trains and scores the model on each of the folds of a k-fold
 *   cross-validation, reporting the score of each fold and their average;
 *   with -i the k models are not trained, they are obtained by unlearning
 *   each fold out of the model of all the samples along the exact solution
 *   path, which is one training plus k walks and makes leave-one-out, i.e.
 *   -k with as many folds as there are samples, affordable; with -u each fold
 *   is instead removed from the model of all the samples and put back, the
 *   Solver following both changes from its previous solution, which is what
 *   any Solver that re-optimizes after a Modification can do;
 *
 * - -P holds out p samples at a time instead of a fold, i.e., it estimates
 *   the leave-p-out error on the subsets that -r asks for, since holding out
 *   each of the binomial(n, p) subsets in turn is out of reach for all but
 *   the smallest data sets;
 *
 * - -r repeats the estimate, each repetition with its own seed and hence with
 *   its own splits, which is what a repeated cross-validation is; the score
 *   reported is the average over all the splits of all the repetitions;
 *
 * - -R asks for the variant in which the bias is regularised with the
 *   weights, i.e., it is one more component of the model and the dual has no
 *   equality constraint, which is the only one LIBLINEAR expresses;
 *
 * - -g compares the hyper-parameters of the given grid, each by
 *   cross-validation (or by the hold-out split of -x), and reports the best;
 *   with -W the values of C that share the other hyper-parameters are walked
 *   in the order the grid gives them, keeping the Solver and re-optimizing
 *   from the previous point, instead of training each point from scratch.
 *
 * - -F asks instead for the recursive elimination of the features: the model
 *   is trained, the features whose weight is smallest are dropped, the model
 *   is trained again on those that are left, and so on until the given number
 *   of them remains, which is a selection over the subsets of the features
 *   done greedily; with -i each round reoptimizes the model of the previous
 *   one, dropping a feature being a rank-one change of the Hessian of the dual
 *   with the linear kernel, and without it each round is a training from
 *   scratch.
 *
 * With --csv the cost of each unit of the model selection, i.e., of each
 * point of the grid on each split, is written as one row of a table, with the
 * seconds it took and the score it gave; the columns that do not change
 * within a run are repeated on every row, so that the files of several runs,
 * e.g., of the same operation with one Solver and with another, concatenate
 * into one table.
 *
 * The score is the accuracy for a classification problem and the coefficient
 * of determination for a regression one, in both cases the larger the better.
 * Every split is stratified on the targets of a classification problem, so
 * that each part keeps the proportions of the two classes, and is drawn out
 * of the seed -e, so that the whole thing is reproducible.
 *
 * The -B and -S options specify, respectively, the BlockConfig and the
 * BlockSolverConfig of the SVMBlock, and are only considered if the given
 * netCDF file is a BlockFile. Note that which formulation of the training
 * problem the abstract representation encodes is precisely what the
 * BlockConfig says [see SVMBlock::generate_abstract_variables()], so it is
 * with -B that one chooses between the Wolfe dual and the primal. The first
 * Solver of the BlockSolverConfig is the one that trains the model.
 *
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

#include <chrono>

#include <cmath>

#include <algorithm>

#include <numeric>

#include <map>
#include <thread>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>

#include <ff/parallel_for.hpp>

// ff/pipeline.hpp declares the static isa2a_get*set() helpers, which are only
// defined in ff/graph_utils.hpp: MSVC rejects the undefined static with C2129
#include <ff/graph_utils.hpp>

#include <SMOSolver.h>
#include <SVCBlock.h>
#include <SVRBlock.h>

#include "common_utils.h"
#include "ml_utils.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

using Index = Block::Index;
using doubleVec = SVMBlock::doubleVec;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

unsigned n_fold = 0;        ///< folds of the cross-validation, 0 = none
bool incremental = false;   ///< a fold is unlearnt rather than trained around
bool warm = false;          ///< the grid is walked along C, reoptimizing
bool reopt = false;         ///< a fold is removed and put back, reoptimizing
double test_fraction = 0;   ///< held-out fraction, 0 = none
std::string grid_spec;      ///< the grid of hyper-parameters to compare
Index n_chunk = 1;          ///< chunks of the consensus rewriting, 1 = none
std::string task = "c";     ///< "c" or "r", only used by the text format
std::string kernel;         ///< name of the kernel, empty = the one of the file
bool libsvm = false;        ///< the text format is the sparse one of LIBSVM
bool reg_bias = false;      ///< the bias is regularised with the weights
unsigned seed = 1;          ///< seed of the splits
long n_jobs = 0;            ///< parallel trainings, 0 = one per core
unsigned repeats = 1;       ///< repetitions of the estimate, one seed each
unsigned leave_out = 0;     ///< samples a leave-p-out holds out, 0 = none
std::string csv_file;       ///< where the cost of each unit is written
std::string rfe_spec;       ///< the elimination of the features, empty = none

/* What each unit of the model selection cost, i.e., one point of the grid on
 * one split: the seconds it took and the name of the Solver that did it. The
 * units are independent and each writes its own slot, so no lock is needed
 * [see evaluate() and evaluate_incremental()]. */

std::vector< double > unit_seconds;   ///< seconds of each unit
std::string solver_name;              ///< the Solver the configuration names

/* What the Solver reported about the last training problem, kept aside for
 * the standard log any other SMS++ tool prints. Model selection trains one
 * model per split and per point of the grid, so this is only reported for
 * the model that is the outcome of the run, i.e., the one that is kept. */

int train_status = Solver::kOK;   ///< status of the last training problem
double train_lb = 0;              ///< lower bound on the last training problem
double train_ub = 0;              ///< upper bound on the last training problem

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/
/// parses a C-string into a value of type T

template< class T >
static void str2num( const char * str , T & value )
{
 std::istringstream( str ) >> value;
 }

/*--------------------------------------------------------------------------*/
/// the value of SVMBlock::kernel_type named by \p name

static int kernel_by_name( const std::string & name )
{
 static const std::pair< const char * , int > names[] = {
   { "linear"    , SVMBlock::kLinear    } ,
   { "poly"      , SVMBlock::kPoly      } ,
   { "gaussian"  , SVMBlock::kGaussian  } ,
   { "laplacian" , SVMBlock::kLaplacian } ,
   { "sigmoid"   , SVMBlock::kSigmoid   } };

 for( const auto & [ n , k ] : names )
  if( name == n )
   return( k );

 std::cerr << "Error: unknown kernel \"" << name << "\"" << std::endl;
 exit( 1 );

 }  // end( kernel_by_name )

/*--------------------------------------------------------------------------*/
/// the SVMBlock of the given file, whatever format it is in

static SVMBlock * read_SVMBlock( void )
{
 // the input file is looked for at the -p prefix, exactly as
 // read_open_netCDF() does with a netCDF one
 const std::string fn = resolve_with_prefix( block_prefix , filename );

 // the plain text format of SVMBlock: try it if the file is not netCDF
 {
  std::ifstream in( fn );
  if( ! in.is_open() ) {
   std::cerr << "Error: cannot open " << fn << std::endl;
   exit( 1 );
   }
  char magic[ 4 ] = { 0 , 0 , 0 , 0 };
  in.read( magic , 3 );
  // every netCDF file starts with "CDF" or, for the HDF5-based netCDF-4,
  // with the \211HDF signature
  if( ( std::string( magic ) != "CDF" ) &&
      ( static_cast< unsigned char >( magic[ 0 ] ) != 0211 ) ) {
   in.clear();
   in.seekg( 0 );
   auto block = dynamic_cast< SVMBlock * >(
    Block::new_Block( ( task == "r" ) ? "SVRBlock" : "SVCBlock" ) );
   block->load( in , libsvm ? 'l' : 0 );
   return( block );
   }
  }

 netCDF::NcFile f;
 auto type = read_open_netCDF( f , filename );

 auto groups = f.getGroups();
 if( groups.empty() ) {
  std::cerr << "Error: " << filename << " contains no Block" << std::endl;
  exit( 1 );
  }

 // only the first Block of the file is trained: unlike a family of unrelated
 // instances, a data set is one thing
 auto & group = groups.begin()->second;

 /* The Block is only read here, not configured: the BlockConfig is what
  * chooses the formulation, and it is applied by train(), once per model
  * trained, together with the BlockSolverConfig. The Configuration a ProbFile
  * carries are therefore ignored, -B and -S being the ones that count. */
 auto block = get_Block( ( type == eProbFile ) ? group.getGroup( "Block" )
                                               : group );

 auto svm = dynamic_cast< SVMBlock * >( block );
 if( ! svm ) {
  std::cerr << "Error: " << filename << " does not contain a SVMBlock"
            << std::endl;
  exit( 1 );
  }

 return( svm );

 }  // end( read_SVMBlock )

/*--------------------------------------------------------------------------*/
/// a SVMBlock holding the samples of \p svm whose index is in \p rows
/** Returns a new SVMBlock of the same class and with the same
 * hyper-parameters as \p svm, but holding only the samples whose index is in
 * \p rows, with the hyper-parameters named by \p grid set to \p point. This
 * is what training on a part of the data set amounts to. */

static SVMBlock * sub_SVMBlock( const SVMBlock * svm , const IndexSet & rows ,
                                const Grid & grid , const GridPoint & point )
{
 auto sub = dynamic_cast< SVMBlock * >(
                                   Block::new_Block( svm->classname() ) );

 double C = svm->get_C() , gamma = svm->get_gamma();
 double coef0 = svm->get_coef0() , epsilon = svm->get_epsilon();
 int degree = svm->get_degree();

 for( std::size_t a = 0 ; a < grid.size() ; ++a ) {
  const auto & name = grid[ a ].first;
  const double value = point[ a ];
  if( name == "C" ) C = value;
  else if( name == "gamma" ) gamma = value;
  else if( name == "degree" ) degree = int( value );
  else if( name == "coef0" ) coef0 = value;
  else if( name == "epsilon" ) epsilon = value;
  else {
   std::cerr << "Error: unknown hyper-parameter \"" << name << "\""
             << std::endl;
   exit( 1 );
   }
  }

 sub->set_C( C );
 sub->set_kernel( svm->get_kernel_type() , gamma , degree , coef0 );
 sub->set_squared_loss( svm->get_squared_loss() );
 sub->set_reg_bias( svm->get_reg_bias() );
 sub->set_K_memory( svm->get_K_memory() );
 sub->set_sparse_density( svm->get_sparse_density() );

 if( auto svr = dynamic_cast< SVRBlock * >( sub ) )
  svr->set_epsilon( epsilon );

 const Index m = svm->get_NFeatures();
 doubleVec X( rows.size() * m ) , y( rows.size() );

 for( std::size_t t = 0 ; t < rows.size() ; ++t ) {
  std::copy_n( svm->get_x( rows[ t ] ) , m , X.begin() + t * m );
  y[ t ] = svm->get_y()[ rows[ t ] ];
  }

 sub->load( rows.size() , m , std::move( X ) , std::move( y ) );

 return( sub );

 }  // end( sub_SVMBlock )

/*--------------------------------------------------------------------------*/
/// the value of the training problem that \p solver reports
/** A Solver that finds a solution reports its value with get_var_value(), but
 * one that rather computes a bound, as any Lagrangian Solver does, leaves it
 * in the finite one of get_lb() and get_ub(): this returns whichever of the
 * three is the value of the training problem. */

static double solver_value( Solver * solver )
{
 const double v = solver->get_var_value();
 if( std::abs( v ) < Inf< double >() )
  return( v );

 const double lb = solver->get_lb() , ub = solver->get_ub();
 if( std::abs( lb ) < Inf< double >() )
  return( lb );

 return( ub );

 }  // end( solver_value )

/*--------------------------------------------------------------------------*/
/// configures \p svm and returns the Solver that has to train it
/** Configures \p svm with the given BlockConfig and BlockSolverConfig, which
 * is also what decides the formulation of the abstract representation, and
 * returns the first Solver the latter has registered. The BlockConfig is
 * consumed, the BlockSolverConfig is the caller's to clean up with.
 *
 * Together with solve() this is the whole of a training, and it only touches
 * \p svm: model selection trains many models at once by calling them from
 * several threads, each with its own copy of the Configuration. They are two
 * because a cross-validation done by unlearning keeps the same Solver
 * attached across the folds [see evaluate_incremental()]. */

static Solver * setup( SVMBlock * svm , Configuration * b_config ,
                       Configuration * s_config )
{

 /* With more than one chunk the training problem is rewritten as one problem
  * per chunk tied by consensus constraints, which is what a Lagrangian Solver
  * attacks: that is a *structure* of the SVMBlock [see
  * SVMBlock::set_structure()], hence it is asked for here and the Solver is
  * attached to the SVMBlock exactly as in the monolithic case. */
 if( n_chunk > 1 ) {
  SimpleConfiguration< int > chunks( n_chunk );
  svm->set_structure( & chunks );
  }

 /* The BlockConfig, which is what chooses the formulation, is applied first
  * and the abstract representation is generated right away, so that the
  * Solver attaches to a Block that is already the formulation it is meant to
  * solve. With no BlockConfig nothing is generated, which is what lets
  * SMOSolver, that does not need the abstract representation, avoid paying
  * for the dense Hessian of the dual it would never look at. */
 if( b_config ) {
  config_Block( svm , b_config , nullptr );
  svm->generate_abstract_variables();
  svm->generate_abstract_constraints();
  svm->generate_objective();
  }

 config_Block( svm , nullptr , s_config );

 auto & solvers = svm->get_registered_solvers();
 if( solvers.empty() ) {
  std::cerr << "Error: the BlockSolverConfig registered no Solver"
            << std::endl;
  exit( 1 );
  }

 set_solver_logs( svm );
 print_solver_parameters( svm );

 return( solvers.front() );

 }  // end( setup )

/*--------------------------------------------------------------------------*/
/// computes with \p solver and reads the trained model back into \p svm
/** Computes the training problem of \p svm with \p solver, which setup() has
 * attached to it, checks the status, reads the model back where the Solver
 * has not written it itself and returns the optimal value. */

static double solve( SVMBlock * svm , Solver * solver )
{
 const int status = solver->compute();

 if( ( status != Solver::kOK ) && ( status != Solver::kLowPrecision ) ) {
  std::cerr << "Error: the Solver returned " << status << std::endl;
  exit( 1 );
  }

 if( solver_name.empty() )
  solver_name = solver->classname();

 train_status = status;
 train_lb = solver->get_lb();
 train_ub = solver->get_ub();

 solver->get_var_solution();

 /* A Solver working on the abstract representation leaves the solution in the
  * Variable, whence the model has to be read back out of them; one that does
  * not need it, such as SMOSolver, has already written the model into the
  * SVMBlock itself, and there is nothing to read. */
 if( svm->get_generated_problem() >= 0 )
  svm->get_solution_from_abstract();

 return( solver_value( solver ) );

 }  // end( solve )

/*--------------------------------------------------------------------------*/
/// trains \p svm with the given Configuration, returning the optimal value

static double train( SVMBlock * svm , Configuration * b_config ,
                     Configuration * s_config )
{
 auto solver = setup( svm , b_config , s_config );
 const double value = solve( svm , solver );

 cleanup_bsc( svm , s_config );
 delete s_config;

 return( value );

 }  // end( train )

/*--------------------------------------------------------------------------*/
/// trains \p svm with the Configuration of -B and -S

static double train( SVMBlock * svm )
{
 return( train( svm , get_config( bconf_file ) , get_config( sconf_file ) ) );
 }

/*--------------------------------------------------------------------------*/
/// writes the trained model of \p svm, if -O asked for it
/** The model is what a SVMBlockSolution saves, which is not what a SVMBlock
 * returns by default [see SVMBlock::get_Solution()]: the Configuration
 * asking for it is therefore passed here, so that -O alone does the obvious
 * thing, unless a -C says otherwise. */

static void write_model( SVMBlock * svm )
{
 if( sol_output.empty() || ( ! sol_cfg_file.empty() ) ) {
  write_final_Solution( svm );
  return;
  }

 SimpleConfiguration< int > cfg( 3 );  // 3 = the trained model
 write_final_Solution( svm , & cfg );

 }  // end( write_model )

/*--------------------------------------------------------------------------*/
/// reports the training problem, in the format every SMS++ tool uses
/** Reports what the Solver said about the training problem of the model that
 * the run produces, i.e., the status and the two bounds, and then the value
 * of \p value, which is the one of the two that is finite. */

static void report_training( double value )
{
 print_status( train_status );
 std::cout << "Upper bound = " << train_ub << std::endl;
 std::cout << "Lower bound = " << train_lb << std::endl;
 std::cout << "training problem: " << std::scientific << std::setprecision( 7 )
           << value << std::endl;

 }  // end( report_training )

/*--------------------------------------------------------------------------*/
/// the predictions of the model of \p model on the given samples of \p svm

static doubleVec predict( const SVMBlock * model , const SVMBlock * svm ,
                          const IndexSet & rows )
{
 doubleVec y_pred( rows.size() );
 for( std::size_t t = 0 ; t < rows.size() ; ++t )
  y_pred[ t ] = model->predict( svm->get_x( rows[ t ] ) );

 return( y_pred );

 }  // end( predict )

/*--------------------------------------------------------------------------*/
/// the true targets of the given samples of \p svm

static doubleVec targets( const SVMBlock * svm , const IndexSet & rows )
{
 doubleVec y( rows.size() );
 for( std::size_t t = 0 ; t < rows.size() ; ++t )
  y[ t ] = svm->get_y()[ rows[ t ] ];

 return( y );

 }  // end( targets )

/*--------------------------------------------------------------------------*/
/// the score of the predictions, the larger the better
/** The accuracy for a classification problem and the coefficient of
 * determination for a regression one. */

static double score( const SVMBlock * svm , const doubleVec & y_true ,
                     const doubleVec & y_pred )
{
 return( dynamic_cast< const SVCBlock * >( svm )
         ? accuracy( y_true , y_pred ) : r2_score( y_true , y_pred ) );
 }

/*--------------------------------------------------------------------------*/
/// the name of the score, for the log

static std::string score_name( const SVMBlock * svm )
{
 return( dynamic_cast< const SVCBlock * >( svm ) ? "accuracy" : "R2" );
 }

/*--------------------------------------------------------------------------*/
/// the score of every point of the grid on every split
/** Trains one model per point of the grid and per split, and returns the
 * score of each of them, the scores of a point being contiguous.
 *
 * The trainings are independent of each other, so they are done in parallel:
 * this is where the time of a model selection goes, and the grid is exactly a
 * cartesian product to spread over the cores. The scores are only reported by
 * the caller, once they are all in, so that the log does not depend on how
 * the work happened to be scheduled.
 *
 * The Configuration are parsed once here and copied for each training, both
 * because parsing them again per model would be pointless and because a
 * Configuration is consumed by the Block it configures. */

/// how many models are trained at once, and with how much memory each
/** How many of the trainings of a model selection are run at once: the cores
 * say how many can run, and the memory says how many can afford to, each
 * model holding a copy of the samples it is trained on and the rows of the
 * Gram matrix its Solver reads. The number is therefore the largest that
 * keeps the whole thing inside half of the memory of the machine, which
 * svm_arch measured at the first compilation [see SVMBlock::dMemory], and
 * never more than the cores; with -j it is what -j says, the user being
 * entitled to know better.
 *
 * What is left of that half once the copies of the samples are paid for is
 * what the Gram matrices may take, split evenly among the models: this is
 * set on \p svm, from which every model of the selection is copied [see
 * sub_SVMBlock()], so that each of them carries its own share. */

static long n_workers( const SVMBlock * svm ,
                       const std::vector< DataSplit > & splits ,
                       std::size_t units )
{
 // hardware_concurrency() may return 0, and a ParallelFor wants at least
 // one worker
 const long cores = std::max< long >( 1 ,
                                      std::thread::hardware_concurrency() );

 // the largest training of the selection is what a worker has to fit
 std::size_t rows = 0;
 for( auto & sp : splits )
  rows = std::max( rows , sp.train.size() );

 const double budget = SVMBlock::dMemory / 2;

 // the copy of the samples, and the one row of the Gram matrix that the
 // cache of a Solver cannot do without
 const double each = double( rows ) * svm->get_NFeatures() * sizeof( double )
                     + double( rows ) * sizeof( double );

 const long fits = ( each > 0 ) ? long( budget / each ) : cores;

 const long workers = ( n_jobs > 0 ) ? n_jobs
                                     : std::max< long >( 1 ,
                                            std::min( { cores , fits ,
                                                        long( units ) } ) );

 /* What is left over is the memory the Gram matrices may use, which is what
  * makes a model selection on a large data set run at all: with none left
  * each Solver keeps the single row it must, and computes the kernel again
  * every time it needs another. */

 const double left = budget - workers * each;
 const_cast< SVMBlock * >( svm )->set_K_memory(
                                   std::max( double( 0 ) , left ) / workers );

 if( workers < cores )
  std::cout << "memory and units allow " << workers
            << ( workers == 1 ? " training" : " trainings" ) << " at once "
            << "out of the " << cores << " the cores would run" << std::endl;

 return( workers );

 }  // end( n_workers )

/*--------------------------------------------------------------------------*/
/// the points of the grid grouped in chains that only differ by C
/** With -W the grid is walked along C: the points that share the value of
 * every other hyper-parameter form a chain, in the order in which the grid
 * lists them, and along a chain the Solver is kept and only told that C has
 * changed, which it follows by re-optimizing from the previous solution
 * [see SVMBlock::set_C()]. Any other hyper-parameter changes the Hessian of
 * the dual, hence each chain starts from scratch. Without -W, or with no C
 * in the grid, each point is a chain of its own. */

static std::vector< std::vector< std::size_t > > chains( const Grid & grid ,
                                     const std::vector< GridPoint > & points )
{
 std::size_t c = grid.size();
 for( std::size_t a = 0 ; a < grid.size() ; ++a )
  if( grid[ a ].first == "C" )
   c = a;

 std::vector< std::vector< std::size_t > > ch;
 if( ( ! warm ) || ( c == grid.size() ) ) {
  for( std::size_t p = 0 ; p < points.size() ; ++p )
   ch.push_back( { p } );
  return( ch );
  }

 std::map< GridPoint , std::size_t > where;
 for( std::size_t p = 0 ; p < points.size() ; ++p ) {
  auto key = points[ p ];
  key[ c ] = 0;
  auto it = where.find( key );
  if( it == where.end() ) {
   where[ key ] = ch.size();
   ch.push_back( { p } );
   }
  else
   ch[ it->second ].push_back( p );
  }

 return( ch );

 }  // end( chains )

/*--------------------------------------------------------------------------*/
/// the value of C at \p point, or the one of \p svm if the grid has none

static double C_of( const SVMBlock * svm , const Grid & grid ,
                    const GridPoint & point )
{
 for( std::size_t a = 0 ; a < grid.size() ; ++a )
  if( grid[ a ].first == "C" )
   return( point[ a ] );

 return( svm->get_C() );
 }

/*--------------------------------------------------------------------------*/

static std::vector< double > evaluate( const SVMBlock * svm ,
                                       const std::vector< DataSplit > &
                                                                    splits ,
                                       const Grid & grid ,
                                       const std::vector< GridPoint > &
                                                                    points )
{
 const std::size_t n_split = splits.size();
 std::vector< double > scores( points.size() * n_split );

 auto b_config = get_config( bconf_file );
 auto s_config = get_config( sconf_file );

 const auto ch = chains( grid , points );
 const long workers = n_workers( svm , splits , ch.size() * n_split );

 /* The chunk of 1: the trainings of a grid have wildly different costs, a
  * large C or a small gamma being much harder than the opposite, so the
  * scheduling has to be dynamic or the workers that drew the easy points
  * would sit idle. What is scheduled is a chain on a split: one point
  * without -W, all the values of C of a row of the grid with it. */
 unit_seconds.assign( scores.size() , 0 );

 ff::ParallelFor pf( workers );
 pf.parallel_for( 0 , ch.size() * n_split , 1 , 1 , [ & ]( const long t ) {
  auto & split = splits[ t % n_split ];
  const auto & chain = ch[ t / n_split ];
  auto start = std::chrono::steady_clock::now();

  auto model = sub_SVMBlock( svm , split.train , grid , points[ chain[ 0 ] ] );
  auto sconf = s_config ? s_config->clone() : nullptr;
  auto solver = setup( model , b_config ? b_config->clone() : nullptr ,
                       sconf );

  for( std::size_t q = 0 ; q < chain.size() ; ++q ) {
   const std::size_t u = chain[ q ] * n_split + t % n_split;
   if( q )
    model->set_C( C_of( svm , grid , points[ chain[ q ] ] ) );

   solve( model , solver );
   scores[ u ] = score( svm , targets( svm , split.test ) ,
                        predict( model , svm , split.test ) );

   const auto now = std::chrono::steady_clock::now();
   unit_seconds[ u ] = std::chrono::duration< double >( now - start ).count();
   start = now;
   }

  cleanup_bsc( model , sconf );
  delete sconf;
  delete model;
  } , workers );

 delete b_config;
 delete s_config;

 return( scores );

 }  // end( evaluate )

/*--------------------------------------------------------------------------*/
/// the same, unlearning each fold out of one model instead of training k
/** Gives what evaluate() gives, but computing the model of a fold the way the
 * incremental and decremental algorithm of [Cauwenberghs and Poggio] allows:
 * ONE model is trained on all the samples, and then, fold by fold, the
 * samples of the fold are unlearnt out of it along the exact solution path,
 * which leaves exactly the model that training on the other folds would have
 * given, the fold is scored on it, and a re-optimization puts them back. A
 * k-fold cross-validation is therefore one training plus k walks rather than
 * k trainings, and leave-one-out, which is `-k n`, becomes affordable.
 *
 * This asks of the Solver something no Solver interface has, unlearning a
 * sample, so it wants a SMOSolver; and it is sequential over the folds by
 * construction, one model being walked back and forth, so what is spread
 * over the cores is the grid. A walk that fails, the system that drives it
 * being singular, costs nothing but the training from scratch of that fold,
 * which is what the other way does anyway. */

static std::vector< double > evaluate_incremental( const SVMBlock * svm ,
                                                   const std::vector<
                                                        DataSplit > & splits ,
                                                   const Grid & grid ,
                                                   const std::vector<
                                                     GridPoint > & points )
{
 const std::size_t n_split = splits.size();
 std::vector< double > scores( points.size() * n_split );
 unit_seconds.assign( scores.size() , 0 );

 auto b_config = get_config( bconf_file );
 auto s_config = get_config( sconf_file );

 IndexSet all( svm->get_NSamples() );
 std::iota( all.begin() , all.end() , 0 );

 const auto ch = chains( grid , points );
 const long workers = n_workers( svm , { DataSplit{ all , IndexSet() } } ,
                                 ch.size() );

 ff::ParallelFor pf( workers );
 pf.parallel_for( 0 , ch.size() , 1 , 1 , [ & ]( const long c ) {
  const auto & chain = ch[ c ];
  auto model = sub_SVMBlock( svm , all , grid , points[ chain[ 0 ] ] );

  auto sconf = s_config ? s_config->clone() : nullptr;
  auto solver = setup( model , b_config ? b_config->clone() : nullptr ,
                       sconf );

  auto smo = dynamic_cast< SMOSolver * >( solver );
  if( ! smo ) {
   std::cerr << "Error: the incremental cross-validation needs a Solver that "
             << "can unlearn a sample, i.e., SMOSolver, and the "
             << "BlockSolverConfig registered a " << solver->classname()
             << std::endl;
   exit( 1 );
   }

  for( std::size_t q = 0 ; q < chain.size() ; ++q ) {
   const std::size_t p = chain[ q ];

   /* The model of all the samples, once per point: at the head of a chain it
    * is a training, further along it is the re-optimization after the change
    * of C. Either way it is part of what the estimate costs, and it is
    * charged to the first fold. */
   auto start = std::chrono::steady_clock::now();
   if( q )
    model->set_C( C_of( svm , grid , points[ p ] ) );
   solve( model , solver );

   for( std::size_t f = 0 ; f < n_split ; ++f ) {
    const auto & fold = splits[ f ].test;
    if( f )
     start = std::chrono::steady_clock::now();

    /* The whole fold goes in one call: unlearning its samples one by one
     * would let each walk give a multiplier back to a sample the previous
     * ones have unlearnt, which the model of the other folds cannot have. */

    const Block::Subset out( fold.begin() , fold.end() );
    const bool walked = ( smo->unlearn( out ) == Solver::kOK );

    if( walked ) {
     solver->get_var_solution();   // the model without the fold, in the Block
     scores[ p * n_split + f ] = score( svm , targets( svm , fold ) ,
                                        predict( model , svm , fold ) );
     }
    else {
     // the path could not be followed: that fold is trained from scratch
     auto sub = sub_SVMBlock( svm , splits[ f ].train , grid , points[ p ] );
     train( sub , b_config ? b_config->clone() : nullptr ,
            s_config ? s_config->clone() : nullptr );
     scores[ p * n_split + f ] = score( svm , targets( svm , fold ) ,
                                        predict( sub , svm , fold ) );
     delete sub;
     }

    /* The fold comes back the way it went out, along the path: putting it
     * back by re-optimizing would be a training, which is exactly what this
     * is here not to do. A walk that fails leaves the multipliers feasible
     * but not optimal, and only a compute() can then put things right. */

    if( ( f + 1 < n_split ) || ( q + 1 < chain.size() ) ) {
     if( ( ! walked ) || ( smo->relearn( out ) != Solver::kOK ) )
      solve( model , solver );
     }

    unit_seconds[ p * n_split + f ] = std::chrono::duration< double >(
                        std::chrono::steady_clock::now() - start ).count();
    }
   }

  cleanup_bsc( model , sconf );
  delete sconf;
  delete model;
  } , workers );

 delete b_config;
 delete s_config;

 return( scores );

 }  // end( evaluate_incremental )

/*--------------------------------------------------------------------------*/
/// the same, removing each fold from one model and putting it back
/** Gives what evaluate() gives, but computing the model of a fold by
 * re-optimization: ONE model is trained on all the samples, and then, fold by
 * fold, the samples of the fold are removed from the SVMBlock, which the
 * Solver learns through the Modification and follows from the previous
 * solution [see SVMBlock::remove_samples()], the fold is scored on the model
 * of the other folds, and it is added back, which the Solver follows the
 * same way [see SVMBlock::add_samples()]. Unlike evaluate_incremental() this
 * asks nothing of the Solver but to react to a Modification, and it is the
 * Solver that decides how: SMOSolver re-optimizes (by iterating, with
 * intSMOPath 0), a Solver with no memory of the previous problem trains
 * again.
 *
 * The removed samples go back at the end of the data set, hence the model
 * keeps the map from its own indices to those of the data set. */

static std::vector< double > evaluate_reopt( const SVMBlock * svm ,
                                             const std::vector< DataSplit > &
                                                                    splits ,
                                             const Grid & grid ,
                                             const std::vector< GridPoint > &
                                                                    points )
{
 const std::size_t n_split = splits.size();
 std::vector< double > scores( points.size() * n_split );
 unit_seconds.assign( scores.size() , 0 );

 auto b_config = get_config( bconf_file );
 auto s_config = get_config( sconf_file );

 const Index n = svm->get_NSamples();
 const Index m = svm->get_NFeatures();
 IndexSet all( n );
 std::iota( all.begin() , all.end() , 0 );

 const auto ch = chains( grid , points );
 const long workers = n_workers( svm , { DataSplit{ all , IndexSet() } } ,
                                 ch.size() );

 ff::ParallelFor pf( workers );
 pf.parallel_for( 0 , ch.size() , 1 , 1 , [ & ]( const long c ) {
  const auto & chain = ch[ c ];
  auto model = sub_SVMBlock( svm , all , grid , points[ chain[ 0 ] ] );
  auto sconf = s_config ? s_config->clone() : nullptr;
  auto solver = setup( model , b_config ? b_config->clone() : nullptr ,
                       sconf );

  IndexSet ord( all );   // the sample of the data set each one of the model is
  IndexSet pos( n );     // and the other way round

  for( std::size_t q = 0 ; q < chain.size() ; ++q ) {
   const std::size_t p = chain[ q ];

   // the model of all the samples, charged to the first fold
   auto start = std::chrono::steady_clock::now();
   if( q )
    model->set_C( C_of( svm , grid , points[ p ] ) );
   solve( model , solver );

   for( std::size_t f = 0 ; f < n_split ; ++f ) {
    const auto & fold = splits[ f ].test;
    if( f )
     start = std::chrono::steady_clock::now();

    for( Index i = 0 ; i < ord.size() ; ++i )
     pos[ ord[ i ] ] = i;

    Block::Subset out( fold.size() );
    for( std::size_t t = 0 ; t < fold.size() ; ++t )
     out[ t ] = pos[ fold[ t ] ];
    std::sort( out.begin() , out.end() );

    std::vector< bool > gone( ord.size() , false );
    for( auto i : out )
     gone[ i ] = true;

    model->remove_samples( std::move( out ) , true );

    IndexSet left;
    left.reserve( ord.size() - fold.size() );
    for( Index i = 0 ; i < ord.size() ; ++i )
     if( ! gone[ i ] )
      left.push_back( ord[ i ] );
    ord = std::move( left );

    solve( model , solver );   // the model of the other folds
    scores[ p * n_split + f ] = score( svm , targets( svm , fold ) ,
                                       predict( model , svm , fold ) );

    // the fold goes back, and the model of all the samples with it
    if( ( f + 1 < n_split ) || ( q + 1 < chain.size() ) ) {
     doubleVec X( fold.size() * m ) , y( fold.size() );
     for( std::size_t t = 0 ; t < fold.size() ; ++t ) {
      std::copy_n( svm->get_x( fold[ t ] ) , m , X.begin() + t * m );
      y[ t ] = svm->get_y()[ fold[ t ] ];
      ord.push_back( fold[ t ] );
      }
     model->add_samples( fold.size() , X , y );
     solve( model , solver );
     }

    unit_seconds[ p * n_split + f ] = std::chrono::duration< double >(
                        std::chrono::steady_clock::now() - start ).count();
    }
   }

  cleanup_bsc( model , sconf );
  delete sconf;
  delete model;
  } , workers );

 delete b_config;
 delete s_config;

 return( scores );

 }  // end( evaluate_reopt )

/*--------------------------------------------------------------------------*/
/// walks the recursive elimination of the features, reporting each round
/** Trains the model, drops the features whose weight is smallest, trains it
 * again on those that are left, and so on until the given number of them
 * remains: this is the recursive feature elimination, which is a model
 * selection over the subsets of the features done greedily, one round per
 * elimination, and it only makes sense with the linear kernel, where a weight
 * is attached to each feature.
 *
 * With -i each round starts from the model of the previous one, the
 * elimination of a feature being a change the Solver follows [see
 * SVMBlock::set_active_features() and SMOSolver::resync_features()]; without
 * it the Solver is thrown away and the round is a training from scratch,
 * which is what the two are compared on. Each round reports the seconds it
 * took and, when a part of the samples is held out, the score of the model on
 * it, so that where the elimination starts to hurt can be read off the
 * table. */

static void rfe( const SVMBlock * svm , const DataSplit * split ,
                 Index keep , Index step )
{
 if( svm->get_kernel_type() != SVMBlock::kLinear ) {
  std::cerr << "Error: the recursive elimination of the features reads the "
            << "weights, which only the linear kernel has" << std::endl;
  exit( 1 );
  }

 const Index m = svm->get_NFeatures();
 const Index n = svm->get_NSamples();

 if( ( ! keep ) || ( keep > m ) ) {
  std::cerr << "Error: the features to keep must be between 1 and " << m
            << std::endl;
  exit( 1 );
  }

 /* The model is trained on the training part of the split and scored on the
  * part that is held out, which is the only honest way of scoring a selection
  * of the features; with no split it is trained on all the samples and only
  * the cost of each round is reported. */

 auto rows = split ? split->train : shuffled_indices( n , 0 );
 auto model = sub_SVMBlock( svm , rows , Grid() , GridPoint() );

 const doubleVec y_true = split ? targets( svm , split->test ) : doubleVec();

 auto s_config = get_config( sconf_file );
 auto solver = setup( model , get_config( bconf_file ) , s_config );

 Block::Subset active( m );
 std::iota( active.begin() , active.end() , Index( 0 ) );

 std::cout << "recursive elimination of the features down to " << keep
           << ", " << step << " at a time"
           << ( incremental ? ", each round reoptimized" : "" ) << std::endl;

 /* Dropping a feature changes the Hessian of the dual as a whole, hence the
  * abstract representation, if there is one, is built anew and whoever reads
  * it is told that nothing of what it had is worth keeping: a Solver that
  * does not read it, such as SMOSolver, can rather reoptimize, but only if it
  * is not generated at all, which is what -B "" asks for. */

 if( incremental && ( model->get_generated_problem() >= 0 ) )
  std::cerr << "Warning: the abstract representation is rebuilt at each "
            << "round, so no Solver can reoptimize; pass -B \"\" to do "
            << "without it" << std::endl;

 std::ofstream csv;
 if( ! csv_file.empty() ) {
  csv.open( csv_file );
  if( ! csv.is_open() ) {
   std::cerr << "Error: cannot write " << csv_file << std::endl;
   exit( 1 );
   }
  csv << "data,samples,features,kernel,solver,estimate,walked,point,split,"
      << "seconds,score" << std::endl;
  }

 for( ; ; ) {
  const auto start = std::chrono::steady_clock::now();
  const double value = solve( model , solver );
  const double seconds = std::chrono::duration< double >(
                          std::chrono::steady_clock::now() - start ).count();

  double sc = 0;
  if( split )
   sc = score( svm , y_true , predict( model , svm , split->test ) );

  std::cout << "  " << std::setw( 6 ) << active.size() << " features  "
            << std::fixed << std::setprecision( 4 ) << seconds << "s  "
            << solver->get_elapsed_iterations() << " iterations  value "
            << std::setprecision( 6 ) << value;
  if( split )
   std::cout << "  " << score_name( svm ) << " " << std::setprecision( 4 )
             << sc;
  std::cout << std::endl;

  if( csv.is_open() )
   csv << filename << "," << rows.size() << "," << active.size() << ","
       << svm->get_kernel_type() << "," << solver_name << ",rfe,"
       << ( incremental ? 1 : 0 ) << ",-,0," << std::fixed
       << std::setprecision( 4 ) << seconds << "," << sc << std::endl;

  if( active.size() <= keep )
   break;

  /* Which features go: those whose weight is smallest in absolute value,
   * which is what the elimination reads as the least useful ones, as many of
   * them per round as the step says and never below what is to be kept. */

  const auto w = model->get_w();
  const Index out = std::min( step , Index( active.size() ) - keep );

  Block::Subset order( active );
  std::partial_sort( order.begin() , order.begin() + out , order.end() ,
                     [ & w ]( Index a , Index b ) {
                      return( std::abs( w[ a ] ) < std::abs( w[ b ] ) ); } );

  Block::Subset gone( order.begin() , order.begin() + out );
  std::sort( gone.begin() , gone.end() );

  Block::Subset left;
  left.reserve( active.size() - out );
  std::set_difference( active.begin() , active.end() , gone.begin() ,
                       gone.end() , std::back_inserter( left ) );
  active = std::move( left );

  model->set_active_features( Block::Subset( active ) );

  /* Without -i the round is a training from scratch: the Solver is detached
   * and another one takes its place, so that it has nothing of the previous
   * round left to start from. */

  if( ! incremental ) {
   cleanup_bsc( model , s_config );
   delete s_config;
   s_config = get_config( sconf_file );
   solver = setup( model , nullptr , s_config );
   }
  }

 cleanup_bsc( model , s_config );
 delete s_config;

 std::cout << "kept:";
 for( auto j : active )
  std::cout << " " << j;
 std::cout << std::endl;

 write_model( model );

 delete model;

 }  // end( rfe )

/*--------------------------------------------------------------------------*/
/// the average of the values

static double mean( const std::vector< double > & v )
{
 double s = 0;
 for( auto x : v )
  s += x;
 return( v.empty() ? 0 : s / v.size() );
 }

/*--------------------------------------------------------------------------*/
/// processes the tool-specific command-line options

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'k' ): str2num( optarg , n_fold );        return( true );
  case( 'i' ): incremental = true;                return( true );
  case( 'W' ): warm = true;                       return( true );
  case( 'u' ): reopt = true;                      return( true );
  case( 'x' ): str2num( optarg , test_fraction ); return( true );
  case( 'g' ): grid_spec = optarg;                return( true );
  case( 't' ): task = optarg;                     return( true );
  case( 'K' ): kernel = optarg;                   return( true );
  case( 'l' ): libsvm = true;                     return( true );
  case( 'e' ): str2num( optarg , seed );          return( true );
  case( 's' ): str2num( optarg , n_chunk );       return( true );
  case( 'j' ): str2num( optarg , n_jobs );        return( true );
  case( 'r' ): str2num( optarg , repeats );       return( true );
  case( 'P' ): str2num( optarg , leave_out );     return( true );
  case( 1001 ): csv_file = optarg;                return( true );
  case( 'R' ): reg_bias = true;                   return( true );
  case( 'F' ): rfe_spec = optarg;                 return( true );
  }

 return( false );

 }  // end( process_specific_arg )

/*--------------------------------------------------------------------------*/
/*-------------------------------- main() ----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 docopt_desc =
  "SMS++ SVM solver: trains a Support Vector Machine (an SVMBlock), and\n"
  "optionally performs the model selection around the training.\n";
 docopt_args =
  "  <file>    the SVMBlock, in an SMS++ netCDF file (.nc4, a Block or a\n"
  "            problem file) or in a plain text format of SVMBlock, the\n"
  "            dense one or, with -l, the sparse one of the LIBSVM data\n"
  "            sets\n";
 docopt_examples =
  "  svm_solver -O model.nc4 data.nc4\n"
  "      train on all the samples and write the trained model\n"
  "  svm_solver -l -k 5 -g \"C=0.1,1,10\" data.txt\n"
  "      compare three values of C by 5-fold cross-validation on a data\n"
  "      set in the LIBSVM format\n"
  "  svm_solver -c myconfig/ data.nc4\n"
  "      use the Configuration files in myconfig/, e.g. a modified copy\n"
  "      of the installed ones\n";

 // Configuration files live in config/ by default; an explicit -c overrides
 // this. Default -B / -S so that a plain run needs neither: SVMCfg.txt is
 // the BlockConfig, which is also what selects the formulation, and
 // SVMSCfg.txt the BlockSolverConfig
 conf_prefix = "config/";
 default_bconf_name = "SVMCfg.txt";
 default_sconf_name = "SVMSCfg.txt";

 short_opts += "k:x:g:t:e:s:j:K:P:r:F:ilRWu";
 const std::vector< option > my_opts = {
   { "kfold"    , required_argument , nullptr , 'k' } ,
   { "holdout"  , required_argument , nullptr , 'x' } ,
   { "grid"     , required_argument , nullptr , 'g' } ,
   { "task"     , required_argument , nullptr , 't' } ,
   { "kernel"   , required_argument , nullptr , 'K' } ,
   { "seed"     , required_argument , nullptr , 'e' } ,
   { "chunks"   , required_argument , nullptr , 's' } ,
   { "jobs"     , required_argument , nullptr , 'j' } ,
   { "leaveout" , required_argument , nullptr , 'P' } ,
   { "repeats"  , required_argument , nullptr , 'r' } ,
   { "csv"      , required_argument , nullptr , 1001 } ,
   { "regbias"  , no_argument       , nullptr , 'R' } ,
   { "rfe"      , required_argument , nullptr , 'F' } ,
   { "increment", no_argument       , nullptr , 'i' } ,
   { "warm"     , no_argument       , nullptr , 'W' } ,
   { "reopt"    , no_argument       , nullptr , 'u' } ,
   { "libsvm"   , no_argument       , nullptr , 'l' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -k, --kfold <n>                 folds of the cross-validation\n"
         "  -i, --increment                 each fold is unlearnt out of one "
         "model\n"
         "                                  instead of training one per fold; "
         "wants\n"
         "                                  SMOSolver\n"
         "  -u, --reopt                     each fold is removed from one "
         "model and\n"
         "                                  put back, the Solver "
         "re-optimizing\n"
         "  -W, --warm                      the grid is walked along C, "
         "each point\n"
         "                                  re-optimized from the previous "
         "one\n"
         "  -x, --holdout <x>               fraction of the samples held out "
         "for the test\n"
         "  -g, --grid <spec>               hyper-parameters to compare, "
         "e.g.\n"
         "                                  \"C=0.1,1,10;gamma=0.5,2\"; the "
         "names are\n"
         "                                  C, gamma, degree, coef0, "
         "epsilon\n"
         "  -R, --regbias                   the bias is regularised with the "
         "weights, i.e.\n"
         "                                  it is one more component of the "
         "model and the\n"
         "                                  dual has no equality "
         "constraint\n"
         "  -l, --libsvm                    the text input is in the sparse "
         "format of\n"
         "                                  the LIBSVM data sets\n"
         "  -t, --task <c|r>                classification or regression, "
         "only for\n"
         "                                  the plain text input format "
         "[c]\n"
         "  -K, --kernel <name>             kernel to train with, one of "
         "linear, poly,\n"
         "                                  gaussian, laplacian and sigmoid; "
         "overrides\n"
         "                                  the one of the file, its "
         "parameters kept\n"
         "  -P, --leaveout <p>              samples held out at a time, i.e. a "
         "leave-p-out\n"
         "                                  estimate on -r subsets rather "
         "than a k-fold\n"
         "  -r, --repeats <n>               repetitions of the estimate, one "
         "seed each,\n"
         "                                  or subsets drawn by -P [1]\n"
         "      --csv <file>                writes what each point of the "
         "grid cost on\n"
         "                                  each split, one row per unit\n"
         "  -F, --rfe <keep[:step]>         recursively eliminates the "
         "features, step of\n"
         "                                  them per round, until keep are "
         "left; with -i\n"
         "                                  each round reoptimizes the "
         "previous model\n"
         "  -e, --seed <n>                  seed of the splits [1]\n"
         "  -s, --chunks <n>                rewrite the training problem as "
         "n chunks tied\n"
         "                                  by consensus constraints, for a "
         "Lagrangian Solver [1]\n"
         "  -j, --jobs <n>                  models trained in parallel during "
         "the model\n"
         "                                  selection [one per core]\n";

 process_args( argc , argv , process_specific_arg );

 auto svm = read_SVMBlock();

 // the kernel of the command line, if any, replaces the one of the file; the
 // Block is not configured yet, so no abstract representation is rebuilt, and
 // the parameters of the kernel are those the file carries unless the grid
 // moves them
 if( ! kernel.empty() )
  svm->set_kernel( kernel_by_name( kernel ) , svm->get_gamma() ,
                   svm->get_degree() , svm->get_coef0() );

 /* Which of the two variants of the bias the instance has is a property of
  * the training problem and not of the Solver, and the Solver that cannot
  * express the one it is given says so [see LIBLINEARSolver, which has the
  * regularised one alone, and LIBSVMSolver, which has the other one]. */
 if( reg_bias )
  svm->set_reg_bias( true );

 const Index n = svm->get_NSamples();

 std::cout << svm->classname() << ": " << n << " samples of "
           << svm->get_NFeatures() << " features" << std::endl;

 // the splits are stratified on the targets of a classification problem
 const doubleVec labels = dynamic_cast< SVCBlock * >( svm ) ? svm->get_y()
                                                            : doubleVec();

 auto grid = parse_grid( grid_spec );
 auto points = grid_points( grid );

 // what the model is scored on: the folds of a cross-validation, a single
 // held-out part, or nothing at all- - - - - - - - - - - - - - - - - - - - -

 std::vector< DataSplit > splits;

 /* A repetition is the same estimate with another seed, and its splits are
  * appended to those of the previous ones: the average over all of them is
  * the repeated estimate, and each score is reported on its own. */
 if( leave_out ) {
  if( n_fold ) {
   std::cerr << "Error: -P and -k are two ways of splitting the samples, "
             << "and only one of them can be asked for" << std::endl;
   exit( 1 );
   }
  splits = leave_p_out( n , leave_out , repeats , seed , labels );
  }
 else
  if( n_fold )
   for( unsigned r = 0 ; r < repeats ; ++r ) {
    auto folds = k_fold( n , n_fold , seed + r , labels );
    splits.insert( splits.end() , folds.begin() , folds.end() );
    }
  else
   if( test_fraction > 0 )
    for( unsigned r = 0 ; r < repeats ; ++r )
     splits.push_back( train_test_split( n , test_fraction , seed + r ,
                                         labels ) );

 /* The recursive elimination of the features is a selection of its own, and
  * it is scored on the part of the samples that -x holds out, if any: the
  * grid, which is a selection over the hyper-parameters, is another matter
  * and the two are not combined. */

 if( ! rfe_spec.empty() ) {
  if( points.size() > 1 ) {
   std::cerr << "Error: -F eliminates the features of one model, hence it "
             << "does not go with a grid of hyper-parameters" << std::endl;
   exit( 1 );
   }

  Index keep = 1 , step = 1;
  const auto colon = rfe_spec.find( ':' );
  str2num( rfe_spec.substr( 0 , colon ).c_str() , keep );
  if( colon != std::string::npos )
   str2num( rfe_spec.substr( colon + 1 ).c_str() , step );

  if( ! step )
   step = 1;

  rfe( svm , splits.empty() ? nullptr : & splits.front() , keep , step );

  delete svm;
  return( 0 );
  }

 if( splits.empty() ) {
  if( points.size() > 1 ) {
   std::cerr << "Error: comparing hyper-parameters needs either -k or -x, "
             << "since a model cannot be selected on the data it is trained "
             << "on" << std::endl;
   exit( 1 );
   }

  // plain training on the whole data set - - - - - - - - - - - - - - - - - -

  report_training( train( svm ) );

  auto y_pred = predict( svm , svm , shuffled_indices( n , 0 ) );
  auto y_true = targets( svm , shuffled_indices( n , 0 ) );
  std::cout << "training " << score_name( svm ) << ": " << std::fixed
            << std::setprecision( 4 ) << score( svm , y_true , y_pred )
            << std::endl;

  write_model( svm );

  delete svm;
  return( 0 );
  }

 // model selection - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 std::cout << ( leave_out
                ? "leave-" + std::to_string( leave_out ) + "-out on "
                  + std::to_string( splits.size() ) + " subsets"
                : ( n_fold ? std::to_string( n_fold ) + "-fold cross-validation"
                           : "hold-out of " + std::to_string( test_fraction ) )
                  + ( repeats > 1 ? " repeated " + std::to_string( repeats )
                                    + " times" : "" ) )
           << ", " << points.size()
           << ( points.size() == 1 ? " point" : " points" ) << " of the grid"
           << ( incremental ? ", each fold unlearnt out of one model" : "" )
           << ( reopt ? ", each fold removed from one model and put back"
                      : "" )
           << std::endl;

 const auto sel_start = std::chrono::steady_clock::now();
 const auto all_scores = incremental
                         ? evaluate_incremental( svm , splits , grid , points )
                         : ( reopt ? evaluate_reopt( svm , splits , grid ,
                                                     points )
                                   : evaluate( svm , splits , grid , points ) );
 const double sel_seconds = std::chrono::duration< double >(
                       std::chrono::steady_clock::now() - sel_start ).count();

 double best_score = - Inf< double >();
 GridPoint best_point;

 for( std::size_t p = 0 ; p < points.size() ; ++p ) {
  const std::vector< double > scores(
                       all_scores.begin() + p * splits.size() ,
                       all_scores.begin() + ( p + 1 ) * splits.size() );
  const double avg = mean( scores );

  std::cout << "  " << score_name( svm ) << " = " << std::fixed
            << std::setprecision( 4 ) << avg;

  if( scores.size() > 1 ) {
   std::cout << "  [";
   for( auto s : scores )
    std::cout << " " << s;
   std::cout << " ]";
   }

  if( ! grid.empty() )
   std::cout << "  ~  " << to_string( grid , points[ p ] );

  std::cout << std::endl;

  if( avg > best_score ) {
   best_score = avg;
   best_point = points[ p ];
   }
  }

 if( points.size() > 1 )
  std::cout << "best: " << score_name( svm ) << " = " << best_score
            << "  ~  " << to_string( grid , best_point ) << std::endl;

 /* The wall clock of the selection includes what the thread pool costs to
  * start, which on a machine with many cores is a sizeable constant (the
  * library reads the topology of each core); the sum of the units is what
  * the trainings and the walks themselves cost, and with one worker it is
  * the figure that compares two ways of doing the same selection. */
 std::cout << "selection: " << std::fixed << std::setprecision( 4 )
           << sel_seconds << " s" << std::endl;
 std::cout << "units: " << std::fixed << std::setprecision( 4 )
           << std::accumulate( unit_seconds.begin() , unit_seconds.end() ,
                               0.0 ) << " s" << std::endl;

 /* What each unit cost, one row per point of the grid and per split: the
  * columns that do not change within a run are repeated on every row, so
  * that the files of several runs concatenate into one table. */
 if( ! csv_file.empty() ) {
  std::ofstream csv( csv_file );
  if( ! csv.is_open() ) {
   std::cerr << "Error: cannot write " << csv_file << std::endl;
   exit( 1 );
   }

  csv << "data,samples,features,kernel,solver,estimate,walked,warm,point,"
      << "split,seconds,score" << std::endl;

  const std::string estimate =
   leave_out ? "leave-" + std::to_string( leave_out ) + "-out"
             : ( n_fold ? std::to_string( n_fold ) + "-fold"
                        : "hold-out" );

  for( std::size_t q = 0 ; q < points.size() ; ++q )
   for( std::size_t f = 0 ; f < splits.size() ; ++f ) {
    const std::size_t t = q * splits.size() + f;
    csv << filename << "," << n << "," << svm->get_NFeatures() << ","
        << svm->get_kernel_type() << "," << solver_name << "," << estimate << ","
        << ( incremental ? 1 : ( reopt ? 2 : 0 ) ) << ","
        << ( warm ? 1 : 0 ) << ",\""
        << ( grid.empty() ? std::string( "-" )
                          : to_string( grid , points[ q ] ) ) << "\","
        << f << "," << std::fixed << std::setprecision( 4 )
        << ( t < unit_seconds.size() ? unit_seconds[ t ] : 0 ) << ","
        << all_scores[ t ] << std::endl;
    }
  }

 /* The selected hyper-parameters are those of the model that is finally
  * trained on the whole data set, which is the model one keeps: the
  * cross-validation is what chooses them, not what produces the model. */
 if( ! grid.empty() ) {
  auto model = sub_SVMBlock( svm , shuffled_indices( n , 0 ) , grid ,
                             best_point );

  std::cout << "retrained on all the samples with "
            << to_string( grid , best_point ) << std::endl;

  report_training( train( model ) );

  write_model( model );

  delete model;
  }
 else
  if( ! sol_output.empty() ) {
   report_training( train( svm ) );
   write_model( svm );
   }

 delete svm;

 return( 0 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File svm_solver.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
