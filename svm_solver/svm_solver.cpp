/*--------------------------------------------------------------------------*/
/*-------------------------- File svm_solver.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * This is a convenient tool for training a Support Vector Machine, i.e., for
 * solving a SVMBlock, and for the model selection that surrounds it.
 *
 * The description of the SVMBlock must be given in a netCDF file, either a
 * BlockFile or a ProbFile, or in the plain text format that SVMBlock reads
 * [see SVMBlock::load( std::istream )], in which case -t says whether it is a
 * classification or a regression problem. This tool can be executed as
 * follows:
 *
 *   ./svm_solver [-B FILE] [-S FILE] [-O FILE] [-k NUMBER] [-x FRACTION]
 *                [-g SPEC] [-t TASK] [-e NUMBER] [-p PATH] [-c PATH]
 *                < file >
 *
 * With none of -k, -x and -g the SVMBlock is simply trained on all of its
 * samples and the value of the training problem is reported; this is what any
 * other SMS++ tool would do, and -O writes the trained model.
 *
 * The other three options are the model selection proper, i.e., the part that
 * is not an optimization problem and that any honest use of a model needs:
 *
 * - -x holds out the given fraction of the samples, trains on the rest and
 *   reports the score on the held-out ones;
 *
 * - -k trains and scores the model on each of the folds of a k-fold
 *   cross-validation, reporting the score of each fold and their average;
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
 * with -B that one chooses between the Wolfe dual, the primal and the
 * decomposed formulation. The first Solver of the BlockSolverConfig is the
 * one that trains the model.
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
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>

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
double test_fraction = 0;   ///< held-out fraction, 0 = none
std::string grid_spec;      ///< the grid of hyper-parameters to compare
Index n_chunk = 1;          ///< chunks of the consensus rewriting, 1 = none
std::string task = "c";     ///< "c" or "r", only used by the text format
unsigned seed = 1;          ///< seed of the splits
std::string model_file;     ///< where the trained model is written

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
/// the SVMBlock of the given file, whatever format it is in

static SVMBlock * read_SVMBlock( void )
{
 // the plain text format of SVMBlock: try it if the file is not netCDF
 {
  std::ifstream in( filename );
  if( ! in.is_open() ) {
   std::cerr << "Error: cannot open " << filename << std::endl;
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
   block->load( in );
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
 Block * block = nullptr;
 Configuration * s_config = nullptr;

 /* The Block is only read here, not configured: the BlockConfig is what
  * chooses the formulation, and it is applied by train(), once per model
  * trained, together with the BlockSolverConfig. */
 if( type == eProbFile )
  get_all( groups.begin()->second , block , s_config );
 else
  get_all( groups.begin()->second , "" , "" , block , s_config );

 delete s_config;

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
/// trains \p svm, returning the value of the training problem
/** Configures \p svm with the BlockConfig and the BlockSolverConfig, which is
 * also what decides the formulation of the abstract representation, computes
 * with the first Solver registered and reads the trained model back into the
 * SVMBlock. */

static double train( SVMBlock * svm )
{
 auto b_config = get_config( bconf_file );
 auto s_config = get_config( sconf_file );

 /* With more than one chunk the training problem is rewritten as one problem
  * per chunk tied by consensus constraints, which is what a Lagrangian Solver
  * attacks; the Solver is then attached to the assembled Block, and the model
  * read out of any of its sub-Block, all of which hold the same one. */
 Block * block = svm;
 if( n_chunk > 1 ) {
  block = make_consensus_Block( svm , n_chunk );
  config_Block( block , nullptr , s_config );

  auto & subsolvers = block->get_registered_solvers();
  if( subsolvers.empty() ) {
   std::cerr << "Error: the BlockSolverConfig registered no Solver"
             << std::endl;
   exit( 1 );
   }

  auto slv = subsolvers.front();
  const int st = slv->compute();
  if( ( st != Solver::kOK ) && ( st != Solver::kLowPrecision ) ) {
   std::cerr << "Error: the Solver returned " << st << std::endl;
   exit( 1 );
   }

  slv->get_var_solution();

  auto sub = dynamic_cast< SVMBlock * >( block->get_nested_Block( 0 ) );
  sub->get_solution_from_abstract();
  svm->set_primal_solution( sub->get_w() , sub->get_b() );

  const double v = solver_value( slv );

  cleanup_bsc( block , s_config );
  delete s_config;
  delete block;

  return( v );
  }

 /* The BlockConfig, which is what chooses the formulation, is applied first
  * and the abstract representation is generated right away, before any Solver
  * is attached: the decomposed formulation creates one sub-Block per chunk,
  * and a Block must not grow new sub-Block while a Solver holds its lock.
  * With no BlockConfig nothing is generated, which is what lets SMOSolver,
  * that does not need the abstract representation, avoid paying for the dense
  * Hessian of the dual it would never look at. */
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

 auto solver = solvers.front();
 const int status = solver->compute();

 if( ( status != Solver::kOK ) && ( status != Solver::kLowPrecision ) ) {
  std::cerr << "Error: the Solver returned " << status << std::endl;
  exit( 1 );
  }

 solver->get_var_solution();

 /* A Solver working on the abstract representation leaves the solution in the
  * Variable, whence the model has to be read back out of them; one that does
  * not need it, such as SMOSolver, has already written the model into the
  * SVMBlock itself, and there is nothing to read. */
 if( svm->get_generated_problem() >= 0 )
  svm->get_solution_from_abstract();

 const double value = solver_value( solver );

 cleanup_bsc( svm , s_config );
 delete s_config;

 return( value );

 }  // end( train )

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
/// trains on each split and returns the score of each of them

static std::vector< double > evaluate( const SVMBlock * svm ,
                                       const std::vector< DataSplit > &
                                                                    splits ,
                                       const Grid & grid ,
                                       const GridPoint & point )
{
 std::vector< double > scores;
 scores.reserve( splits.size() );

 for( auto & split : splits ) {
  auto model = sub_SVMBlock( svm , split.train , grid , point );
  train( model );
  scores.push_back( score( svm , targets( svm , split.test ) ,
                           predict( model , svm , split.test ) ) );
  delete model;
  }

 return( scores );

 }  // end( evaluate )

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
  case( 'x' ): str2num( optarg , test_fraction ); return( true );
  case( 'g' ): grid_spec = optarg;                 return( true );
  case( 't' ): task = optarg;                      return( true );
  case( 'e' ): str2num( optarg , seed );          return( true );
  case( 's' ): str2num( optarg , n_chunk );       return( true );
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

 docopt_desc = "SMS++ SVM solver.\n";

 // Configuration files live in config/ by default; an explicit -c overrides
 // this. Default -B / -S so that a plain run needs neither: SVMCfg.txt is
 // the BlockConfig, which is also what selects the formulation, and
 // SVMSCfg.txt the BlockSolverConfig
 conf_prefix = "config/";
 default_bconf_name = "SVMCfg.txt";
 default_sconf_name = "SVMSCfg.txt";

 short_opts += "k:x:g:t:e:s:";
 const std::vector< option > my_opts = {
   { "kfold"    , required_argument , nullptr , 'k' } ,
   { "holdout"  , required_argument , nullptr , 'x' } ,
   { "grid"     , required_argument , nullptr , 'g' } ,
   { "task"     , required_argument , nullptr , 't' } ,
   { "seed"     , required_argument , nullptr , 'e' } ,
   { "chunks"   , required_argument , nullptr , 's' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -k, --kfold <n>                 folds of the cross-validation\n"
         "  -x, --holdout <x>               fraction of the samples held out "
         "for the test\n"
         "  -g, --grid <spec>               hyper-parameters to compare, "
         "e.g.\n"
         "                                  \"C=0.1,1,10;gamma=0.5,2\"; the "
         "names are\n"
         "                                  C, gamma, degree, coef0, "
         "epsilon\n"
         "  -t, --task <c|r>                classification or regression, "
         "only for\n"
         "                                  the plain text input format "
         "[c]\n"
         "  -e, --seed <n>                  seed of the splits [1]\n"
         "  -s, --chunks <n>                rewrite the training problem as "
         "n chunks tied\n"
         "                                  by consensus constraints, for a "
         "Lagrangian Solver [1]\n";

 process_args( argc , argv , process_specific_arg );

 auto svm = read_SVMBlock();

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

  const double value = train( svm );

  std::cout << "training problem: " << std::scientific
           << std::setprecision( 7 ) << value << std::endl;

  auto y_pred = predict( svm , svm , shuffled_indices( n , 0 ) );
  auto y_true = targets( svm , shuffled_indices( n , 0 ) );
  std::cout << "training " << score_name( svm ) << ": " << std::fixed
            << std::setprecision( 4 ) << score( svm , y_true , y_pred )
            << std::endl;

  if( output_solution )
   write_final_Solution( svm );

  delete svm;
  return( 0 );
  }

 // model selection - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 std::cout << ( n_fold ? std::to_string( n_fold ) + "-fold cross-validation"
                       : "hold-out of " + std::to_string( test_fraction ) )
           << ", " << points.size()
           << ( points.size() == 1 ? " point" : " points" ) << " of the grid"
           << std::endl;

 double best_score = - Inf< double >();
 GridPoint best_point;

 for( auto & point : points ) {
  auto scores = evaluate( svm , splits , grid , point );
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
   std::cout << "  ~  " << to_string( grid , point );

  std::cout << std::endl;

  if( avg > best_score ) {
   best_score = avg;
   best_point = point;
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
  train( model );

  std::cout << "retrained on all the samples with "
            << to_string( grid , best_point ) << std::endl;

  if( output_solution )
   write_final_Solution( model );

  delete model;
  }
 else
  if( output_solution ) {
   train( svm );
   write_final_Solution( svm );
   }

 delete svm;

 return( 0 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File svm_solver.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
