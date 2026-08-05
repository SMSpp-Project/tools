/*--------------------------------------------------------------------------*/
/*----------------------------- ml_utils.h ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Model-agnostic scaffolding for the machine learning tools of SMS++.
 *
 * Training a machine learning model is an optimization problem, which is what
 * SMS++ is for; *selecting* one is not, but it is what surrounds the training
 * in any honest use of a model: the data set has to be split, the model has
 * to be scored on data it has not been trained on, and its hyper-parameters
 * have to be chosen by comparing such scores. This file provides exactly that
 * scaffolding, and nothing else:
 *
 * - splitting a data set, either once (hold-out) or into the folds of a
 *   k-fold cross-validation, optionally stratified so that each part keeps
 *   the proportions of the classes;
 *
 * - the scores themselves, i.e., the accuracy for a classifier and the mean
 *   squared error, the mean absolute error and the coefficient of
 *   determination for a regression;
 *
 * - the grid of hyper-parameter values to be compared, its parsing out of a
 *   command-line string and its enumeration.
 *
 * Everything here is deliberately model-agnostic and depends on nothing but
 * the standard library: it knows about indices of samples and vectors of
 * numbers, never about Block, Solver or any specific model, so that a tool
 * for a different model can use it unchanged. What each tool has to provide
 * is the one thing that is not scaffolding, i.e., how a model is trained on a
 * subset of the samples and evaluated on another one.
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*----------------------------- DEFINITIONS --------------------------------*/
/*--------------------------------------------------------------------------*/

#ifndef __TOOLS_ML_UTILS
 #define __TOOLS_ML_UTILS

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <cstddef>

#include <string>

#include <vector>

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/

/// a set of indices of samples of a data set

using IndexSet = std::vector< std::size_t >;

/*--------------------------------------------------------------------------*/
/// a partition of (part of) a data set into a training and a held-out part

struct DataSplit {
 IndexSet train;  ///< the indices the model is trained on
 IndexSet test;   ///< the indices the model is scored on
 };

/*--------------------------------------------------------------------------*/
/// one axis of a grid of hyper-parameter values: its name and its values

using GridAxis = std::pair< std::string , std::vector< double > >;

/// a grid of hyper-parameter values, i.e., one axis per hyper-parameter

using Grid = std::vector< GridAxis >;

/// one point of a grid: one value per axis, in the order of the axes

using GridPoint = std::vector< double >;

/*--------------------------------------------------------------------------*/
/*------------------------- SPLITTING THE DATA -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @name Splitting the data set
 *  @{ */

/// a pseudo-random permutation of the indices 0, ..., n - 1
/** Returns a pseudo-random permutation of 0, ..., n - 1 drawn out of \p seed,
 * hence the same seed always gives the same permutation, which is what makes
 * any experiment based on it reproducible.
 *
 * The permutation is specified down to the bit, rather than being left to
 * whatever the standard library happens to do, so that it is the same one on
 * every platform and can be reproduced in any other language, which is what
 * it takes to compare an experiment with the same experiment run elsewhere:
 *
 * - the pseudo-random numbers are those of *splitmix64*, i.e. the sequence
 *   \f$ z_t \f$ obtained from the state
 *   \f$ s_t = s_{t-1} + \f$ 0x9E3779B97F4A7C15, \f$ s_0 = \f$ \p seed, by
 *   \f$ z = ( s \oplus ( s \gg 30 ) ) \cdot \f$ 0xBF58476D1CE4E5B9,
 *   \f$ z = ( z \oplus ( z \gg 27 ) ) \cdot \f$ 0x94D049BB133111EB and
 *   \f$ z = z \oplus ( z \gg 31 ) \f$, all in 64 unsigned bits;
 *
 * - a number below a bound is drawn out of them by rejection, discarding the
 *   values below \f$ 2^{64} \bmod \f$ bound and taking the remainder of the
 *   first one that survives, which is unbiased;
 *
 * - the permutation is the Fisher-Yates shuffle running downwards, i.e. for
 *   \f$ i = n - 1 , \dots , 1 \f$ the entries \f$ i \f$ and \f$ j \f$
 *   are swapped, \f$ j \f$ being drawn below \f$ i + 1 \f$. */

IndexSet shuffled_indices( std::size_t n , unsigned seed );

/*--------------------------------------------------------------------------*/
/// splits a data set into a training and a held-out part
/** Splits the indices of the \p n samples into a training and a held-out
 * part, the latter being the fraction \p test_fraction of the whole, drawn
 * out of \p seed.
 *
 * If \p labels is not empty the split is *stratified* on it, i.e., the
 * samples are dealt out label by label, so that each part keeps the
 * proportions of each class; this is what one wants for a classification
 * problem, all the more so for an unbalanced one, and it is a harmless
 * reordering for a regression one. Pass an empty vector for the plain
 * split. */

DataSplit train_test_split( std::size_t n , double test_fraction ,
                            unsigned seed ,
                            const std::vector< double > & labels = {} );

/*--------------------------------------------------------------------------*/
/// the folds of a k-fold cross-validation of a data set
/** Returns the \p k folds of a cross-validation of the \p n samples: the
 * i-th DataSplit has as its held-out part the i-th fold and as its training
 * part all the others, so that every sample is held out exactly once. The
 * folds are drawn out of \p seed and, if \p labels is not empty, they are
 * stratified on it exactly as in train_test_split(). */

std::vector< DataSplit > k_fold( std::size_t n , unsigned k , unsigned seed ,
                                 const std::vector< double > & labels = {} );

/** @} ---------------------------------------------------------------------*/
/*------------------------------- SCORES -----------------------------------*/
/*--------------------------------------------------------------------------*/
/** @name Scoring a model
 *
 * Each score takes the true targets and the predicted ones, which must have
 * the same size, and is oriented so that *larger is better*: this is what
 * lets the model selection compare hyper-parameters without knowing which
 * score it is looking at, and it is why the error-based ones are returned
 * negated by neg_mean_squared_error() and neg_mean_absolute_error().
 *  @{ */

/// the fraction of the samples whose predicted class is the true one

double accuracy( const std::vector< double > & y_true ,
                 const std::vector< double > & y_pred );

/// the mean of the squared errors

double mean_squared_error( const std::vector< double > & y_true ,
                           const std::vector< double > & y_pred );

/// the mean of the absolute errors

double mean_absolute_error( const std::vector< double > & y_true ,
                            const std::vector< double > & y_pred );

/// minus the mean of the squared errors, so that larger is better

double neg_mean_squared_error( const std::vector< double > & y_true ,
                               const std::vector< double > & y_pred );

/// minus the mean of the absolute errors, so that larger is better

double neg_mean_absolute_error( const std::vector< double > & y_true ,
                                const std::vector< double > & y_pred );

/// the coefficient of determination, i.e., 1 - SS_res / SS_tot
/** The fraction of the variance of the targets that the model explains: 1 is
 * a perfect fit, 0 is what predicting the mean of the targets would score,
 * and a negative value means that the model does worse than that. It is
 * undefined, and returned as 0, if all the targets are equal. */

double r2_score( const std::vector< double > & y_true ,
                 const std::vector< double > & y_pred );

/** @} ---------------------------------------------------------------------*/
/*-------------------------------- GRID ------------------------------------*/
/*--------------------------------------------------------------------------*/
/** @name The grid of hyper-parameter values
 *  @{ */

/// parses a grid out of its command-line description
/** Parses a Grid out of a string of the form
 *
 *     name=v1,v2,...[;name=v1,v2,...]...
 *
 * e.g. "C=0.1,1,10;gamma=0.5,1,2", whose axes are taken in the order they
 * appear. Throws exception if the string is malformed; an empty string gives
 * an empty Grid, whose only point is the empty one, i.e., "leave every
 * hyper-parameter as it is". */

Grid parse_grid( const std::string & spec );

/*--------------------------------------------------------------------------*/
/// enumerates the points of a grid
/** Returns all the points of the Cartesian product of the axes of \p grid,
 * with the *last* axis varying fastest. An empty Grid gives exactly one,
 * empty, point, so that the caller needs no special case for it. */

std::vector< GridPoint > grid_points( const Grid & grid );

/*--------------------------------------------------------------------------*/
/// writes a point of a grid as "name = value, ..."

std::string to_string( const Grid & grid , const GridPoint & point );

/** @} ---------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

#endif  /* __TOOLS_ML_UTILS */

/*--------------------------------------------------------------------------*/
/*--------------------------- End ml_utils.h -------------------------------*/
/*--------------------------------------------------------------------------*/
