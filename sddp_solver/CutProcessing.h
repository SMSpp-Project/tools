/*--------------------------------------------------------------------------*/
/*----------------------- File CutProcessing.h -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Header file of CutProcessing, a class for processing the cuts of an
 * SDDPBlock.
 *
 * \author Rafael Durbano Lobato \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Rafael Durbano Lobato
 */
/*--------------------------------------------------------------------------*/
/*----------------------------- DEFINITIONS --------------------------------*/
/*--------------------------------------------------------------------------*/

#ifndef __CutProcessing
#define __CutProcessing
                      /* self-identification: #endif at the end of the file */

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <PolyhedralFunction.h>
#include <SDDPBlock.h>

/*--------------------------------------------------------------------------*/
/*--------------------------- NAMESPACE ------------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------ CLASS CutProcessing -----------------------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------- GENERAL NOTES --------------------------------*/
/*--------------------------------------------------------------------------*/
/// a class for processing cuts of an SDDPBlock
/** The CutProcessing class implements some methods to eliminate redundant
 * cuts of an SDDPBlock (or redundant rows of a PolyhedralFunction). Cuts in
 * an SDDPBlock are represented by rows of a PolyhedralFunction. A
 * PolyhedralFunction represents a function of the form
 *
 *     f( x ) = max { a_i x + b_i : i in { 0 , ... , m - 1 } }
 *
 * in the convex case (pointwise maximum of linear functions), and
 *
 *     f( x ) = min { a_i x + b_i : i in { 0 , ... , m - 1 } }
 *
 * in the concave case (pointwise minimum of linear functions). This class
 * implements methods that eliminates two types of redundant cuts (or rows):
 * parallel cuts and inactive ones. Let s = - 1 if the PolyhedralFunction is
 * convex and s = 1 if it is concave. If there exist i and k such that i != k,
 * a_i is equal to a_k, and s * b_k >= s * b_i, then these two cuts are
 * parallel and k is redundant (since cut i dominates cut k). A cut k is
 * inactive if the following linear programming problem has a finite optimal
 * value that is strictly less than s * b_k.
 *
 *     max   s * ( y - a_k x )
 *     s.t.  s * ( y - a_i x ) <= s * b_i , i in { 0 , ... , m - 1 } \ { k }.
 *
 * In this case, cut k is also redundant.
 *
 * Two parameters can be used in order to determine if two cuts are parallel:
 * parallel_error and parallel_relative_error. We consider that two cuts i and
 * k are parallel if, for every component j,
 *
 *     | a_{ij} - a_{kj} | <= parallel_error
 *
 * and
 *
 *     | a_{ij} - a_{kj} | <= parallel_relative_error *
 *                            max{ | a_{ij} | , | a_{kj} | }.
 *
 * Two parameters can also be used to determine if a cut is inactive:
 * optimization_error and optimization_relative_error. We consider that a cut
 * k is inactive if the linear programming problem above has an optimal
 * solution whose value (objective_value) is such that
 *
 *     objective_value < s * b_k + optimization_error
 *
 * and
 *
 *     objective_value < s * b_k + optimization_relative_error *
 *                                 max( | objective_value | , | b_k | ).
 *
 * All of these parameters have 0 as default value.
 *
 * A configuration file can be used to configure the Solver for the linear
 * programming problem above, which is represented by an AbstractBlock. The
 * name of the configuration file can be specified by the function
 * set_config_filename(). If no configuration file is provided, then a
 * CPXMILPSolver is used to solve that problem.
 */

class CutProcessing {

/*--------------------------------------------------------------------------*/
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/
/*--------------------------------------------------------------------------*/

public:

/*--------------------------------------------------------------------------*/
/*--------------------- PUBLIC METHODS OF THE CLASS ------------------------*/
/*--------------------------------------------------------------------------*/

 void remove_parallel_cuts( PolyhedralFunction * function ) const;

/*--------------------------------------------------------------------------*/

 void remove_parallel_cuts( SDDPBlock * sddp_block ) const;

/*--------------------------------------------------------------------------*/

 void remove_redundant_cuts( PolyhedralFunction * function ) const;

/*--------------------------------------------------------------------------*/

 void remove_redundant_cuts( SDDPBlock * sddp_block ) const;

/*--------------------------------------------------------------------------*/

 void set_parallel_error( double error ) {
  parallel_error = error;
 }

/*--------------------------------------------------------------------------*/

 void set_parallel_relative_error( double error ) {
  parallel_relative_error = error;
 }

/*--------------------------------------------------------------------------*/

 void set_optimization_error( double error ) {
  optimization_error = error;
 }

/*--------------------------------------------------------------------------*/

 void set_optimization_relative_error( double error ) {
  optimization_relative_error = error;
 }

/*--------------------------------------------------------------------------*/

 void set_config_filename( const std::string & filename ) {
  config_filename = filename;
 }

/*--------------------------------------------------------------------------*/
/*---------------------- PROTECTED PART OF THE CLASS -----------------------*/
/*--------------------------------------------------------------------------*/

protected:

/*--------------------------------------------------------------------------*/
/*--------------------- PROTECTED FIELDS OF THE CLASS ----------------------*/
/*--------------------------------------------------------------------------*/

 double parallel_error = 0.0;

 double parallel_relative_error = 0.0;

 double optimization_error = 0.0;

 double optimization_relative_error = 0.0;

 std::string config_filename;

};

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

#endif  /* CutProcessing.h included */

/*--------------------------------------------------------------------------*/
/*---------------------- End File CutProcessing.h --------------------------*/
/*--------------------------------------------------------------------------*/
