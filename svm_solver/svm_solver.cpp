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
 *   -k with as many folds as there are samples, affordable;
 *
 * - -g compares the hyper-parameters of the given grid, each by
 *   cross-validation (or by the hold-out split of -x), and reports the best.
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

#include <cmath>

#include <numeric>

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
double test_fraction = 0;   ///< held-out fraction, 0 = none
std::string grid_spec;      ///< the grid of hyper-parameters to compare
Index n_chunk = 1;          ///< chunks of the consensus rewriting, 1 = none
std::string task = "c";     ///< "c" or "r", only used by the text format
std::string kernel;         ///< name of the kernel, empty = the one of the file
bool libsvm = false;        ///< the text format is the sparse one of LIBSVM
unsigned seed = 1;          ///< seed of the splits
long n_jobs = 0;            ///< parallel trainings, 0 = one per core

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

 // hardware_concurrency() may return 0, and a ParallelFor wants at least
 // one worker
 const long workers = n_jobs > 0 ? n_jobs
  : std::max< long >( 1 , std::thread::hardware_concurrency() );

 /* The chunk of 1: the trainings of a grid have wildly different costs, a
  * large C or a small gamma being much harder than the opposite, so the
  * scheduling has to be dynamic or the workers that drew the easy points
  * would sit idle. */
 ff::ParallelFor pf( workers );
 pf.parallel_for( 0 , scores.size() , 1 , 1 , [ & ]( const long t ) {
  auto & split = splits[ t % n_split ];

  auto model = sub_SVMBlock( svm , split.train , grid ,
                             points[ t / n_split ] );

  train( model , b_config ? b_config->clone() : nullptr ,
         s_config ? s_config->clone() : nullptr );

  scores[ t ] = score( svm , targets( svm , split.test ) ,
                       predict( model , svm , split.test ) );
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

 auto b_config = get_config( bconf_file );
 auto s_config = get_config( sconf_file );

 IndexSet all( svm->get_NSamples() );
 std::iota( all.begin() , all.end() , 0 );

 const long workers = n_jobs > 0 ? n_jobs
  : std::max< long >( 1 , std::thread::hardware_concurrency() );

 ff::ParallelFor pf( workers );
 pf.parallel_for( 0 , points.size() , 1 , 1 , [ & ]( const long p ) {
  auto model = sub_SVMBlock( svm , all , grid , points[ p ] );

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

  solve( model , solver );   // the model of all the samples, once

  for( std::size_t f = 0 ; f < n_split ; ++f ) {
   const auto & fold = splits[ f ].test;

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

   if( f + 1 < n_split ) {
    if( ( ! walked ) || ( smo->relearn( out ) != Solver::kOK ) )
     solve( model , solver );
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
  case( 'x' ): str2num( optarg , test_fraction ); return( true );
  case( 'g' ): grid_spec = optarg;                return( true );
  case( 't' ): task = optarg;                     return( true );
  case( 'K' ): kernel = optarg;                   return( true );
  case( 'l' ): libsvm = true;                     return( true );
  case( 'e' ): str2num( optarg , seed );          return( true );
  case( 's' ): str2num( optarg , n_chunk );       return( true );
  case( 'j' ): str2num( optarg , n_jobs );        return( true );
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

 short_opts += "k:x:g:t:e:s:j:K:il";
 const std::vector< option > my_opts = {
   { "kfold"    , required_argument , nullptr , 'k' } ,
   { "holdout"  , required_argument , nullptr , 'x' } ,
   { "grid"     , required_argument , nullptr , 'g' } ,
   { "task"     , required_argument , nullptr , 't' } ,
   { "kernel"   , required_argument , nullptr , 'K' } ,
   { "seed"     , required_argument , nullptr , 'e' } ,
   { "chunks"   , required_argument , nullptr , 's' } ,
   { "jobs"     , required_argument , nullptr , 'j' } ,
   { "increment", no_argument       , nullptr , 'i' } ,
   { "libsvm"   , no_argument       , nullptr , 'l' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -k, --kfold <n>                 folds of the cross-validation\n"
         "  -i, --increment                 each fold is unlearnt out of one "
         "model\n"
         "                                  instead of training one per fold; "
         "wants\n"
         "                                  SMOSolver\n"
         "  -x, --holdout <x>               fraction of the samples held out "
         "for the test\n"
         "  -g, --grid <spec>               hyper-parameters to compare, "
         "e.g.\n"
         "                                  \"C=0.1,1,10;gamma=0.5,2\"; the "
         "names are\n"
         "                                  C, gamma, degree, coef0, "
         "epsilon\n"
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

 if( n_fold )
  splits = k_fold( n , n_fold , seed , labels );
 else
  if( test_fraction > 0 )
   splits.push_back( train_test_split( n , test_fraction , seed , labels ) );

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

 std::cout << ( n_fold ? std::to_string( n_fold ) + "-fold cross-validation"
                       : "hold-out of " + std::to_string( test_fraction ) )
           << ", " << points.size()
           << ( points.size() == 1 ? " point" : " points" ) << " of the grid"
           << ( incremental ? ", each fold unlearnt out of one model" : "" )
           << std::endl;

 const auto all_scores = incremental
                         ? evaluate_incremental( svm , splits , grid , points )
                         : evaluate( svm , splits , grid , points );

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
