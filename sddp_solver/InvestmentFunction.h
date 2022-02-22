/*--------------------------------------------------------------------------*/
/*----------------------- File InvestmentFunction.h ------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Header file for the class InvestmentFunction, which implements C05Function
 * and Block and represents a function that can compute the value of an
 * investment in a set of assets.
 *
 * \author Rafael Durbano Lobato \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Rafael Durbano Lobato.
 */
/*--------------------------------------------------------------------------*/
/*----------------------------- DEFINITIONS --------------------------------*/
/*--------------------------------------------------------------------------*/

#ifndef __InvestmentFunction
#define __InvestmentFunction
                      /* self-identification: #endif at the end of the file */

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "Block.h"
#include "C05Function.h"
#include "CDASolver.h"

/*--------------------------------------------------------------------------*/
/*--------------------------- NAMESPACE ------------------------------------*/
/*--------------------------------------------------------------------------*/

/// namespace for the Structured Modeling System++ (SMS++)
namespace SMSpp_di_unipi_it
{
 class BendersBFunction;       // forward declaration of BendersBFunction

 class IntermittentUnitBlock;  // forward declaration of IntermittentUnitBlock

 class UCBlock;                // forward declaration of UClock

/*--------------------------------------------------------------------------*/
/*------------------------------- CLASSES ----------------------------------*/
/*--------------------------------------------------------------------------*/
/** @defgroup InvestmentFun_CLASSES Classes in InvestmentFunction.h
 *  @{ */

/*--------------------------------------------------------------------------*/
/*----------------------- CLASS InvestmentFunction -------------------------*/
/*--------------------------------------------------------------------------*/
/*--------------------------- GENERAL NOTES --------------------------------*/
/*--------------------------------------------------------------------------*/
/// a Function for scaling a set of UnitBlock
/** The class InvestmentFunction represents a C05Function that is capable of
 * evaluating the investment on a set of assets. InvestmentFunction derives
 * from *both* C05Function and Block.
 *
 * The main ingredients of an InvestmentFunction are the following:
 *
 * TODO
 */

class InvestmentFunction : public C05Function , public Block {

/*--------------------------------------------------------------------------*/
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/
/*--------------------------------------------------------------------------*/

 public:

/*--------------------------------------------------------------------------*/
/*---------------------- PUBLIC TYPES OF THE CLASS -------------------------*/
/*--------------------------------------------------------------------------*/
/** @name Public Types
 *  @{ */

 /* Since InvestmentFunction is both a ThinVarDepInterface and a Block, it
  * "sees" two definitions of "Index", "Range", and "Subset". These are
  * actually the same, but compilers still don't like it. Disambiguate by
  * declaring we use the ThinVarDepInterface versions (but it could have been
  * the Block versions, as they are the same). */

 using Index    = ThinVarDepInterface::Index;
 using c_Index  = ThinVarDepInterface::c_Index;

 using Range    = ThinVarDepInterface::Range;
 using c_Range  = ThinVarDepInterface::c_Range;

 using Subset   = ThinVarDepInterface::Subset;
 using c_Subset = ThinVarDepInterface::c_Subset;

 using IndexVector = std::vector< Index >;
 using RealVector = std::vector< double >;

 using VarVector = std::vector< ColVariable * >;
 ///< representing the x variables upon which the function depends

/*--------------------------------------------------------------------------*/
 /// virtualized concrete iterator
 /** A concrete class deriving from ThinVarDepInterface::v_iterator and
  * implementing the concrete iterator for sifting through the "active"
  * Variable of an InvestmentFunction. */

 class v_iterator : public ThinVarDepInterface::v_iterator
 {
  public:

  explicit v_iterator( VarVector::iterator & itr ) : itr_( itr ) {}
  explicit v_iterator( VarVector::iterator && itr )
   : itr_( std::move( itr ) ) {}

  v_iterator * clone( void ) override final {
   return( new v_iterator( itr_ ) );
   }

  void operator++( void ) override final { ++(itr_); }

  reference operator*( void ) const override final {
   return( *((*itr_)) );
   }
  pointer operator->( void ) const override final {
   return( (*itr_) );
   }

  bool operator==( const ThinVarDepInterface::v_iterator & rhs )
   const override final {
   #ifdef NDEBUG
    auto tmp = static_cast<const InvestmentFunction::v_iterator *>( & rhs );
    return( itr_ == tmp->itr_ );
   #else
    auto tmp = dynamic_cast<const InvestmentFunction::v_iterator *>( & rhs );
    return( tmp ? itr_ == tmp->itr_ : false );
   #endif
   }
  bool operator!=( const ThinVarDepInterface::v_iterator & rhs )
   const override final {
   #ifdef NDEBUG
    auto tmp = static_cast<const InvestmentFunction::v_iterator *>( & rhs );
    return( itr_ != tmp->itr_ );
   #else
    auto tmp = dynamic_cast<const InvestmentFunction::v_iterator *>( & rhs );
    return( tmp ? itr_ != tmp->itr_ : true );
   #endif
   }

  private:

  VarVector::iterator itr_;
  };

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
 /// virtualized concrete const_iterator
 /** A concrete class deriving from ThinVarDepInterface::v_const_iterator and
  * implementing the concrete iterator for sifting through the "active"
  * Variable of an InvestmentFunction. */

 class v_const_iterator : public ThinVarDepInterface::v_const_iterator
 {
  public:

  explicit v_const_iterator( VarVector::const_iterator & itr ) : itr_( itr ) {}
  explicit v_const_iterator( VarVector::const_iterator && itr )
   : itr_( std::move( itr ) ) {}

  v_const_iterator * clone( void ) override final {
   return( new v_const_iterator( itr_ ) );
   }

  void operator++( void ) override final { ++(itr_); }

  reference operator*( void ) const override final { return( *((*itr_)) ); }
  pointer operator->( void ) const override final { return( (*itr_) ); }

  bool operator==( const ThinVarDepInterface::v_const_iterator & rhs )
   const override final {
   #ifdef NDEBUG
    auto tmp = static_cast<const InvestmentFunction::v_const_iterator *>(
								      & rhs );
    return( itr_ == tmp->itr_ );
   #else
    auto tmp = dynamic_cast<const InvestmentFunction::v_const_iterator *>(
								      & rhs );
    return( tmp ? itr_ == tmp->itr_ : false );
   #endif
   }
  bool operator!=( const ThinVarDepInterface::v_const_iterator & rhs )
   const override final {
   #ifdef NDEBUG
    auto tmp = static_cast<const InvestmentFunction::v_const_iterator *>(
								      & rhs );
    return( itr_ != tmp->itr_ );
   #else
    auto tmp = dynamic_cast<const InvestmentFunction::v_const_iterator *>(
								      & rhs );
    return( tmp ? itr_ != tmp->itr_ : true );
   #endif
   }

  private:

  VarVector::const_iterator itr_;
  };

/**@} ----------------------------------------------------------------------*/
/*------------- CONSTRUCTING AND DESTRUCTING InvestmentFunction ------------*/
/*--------------------------------------------------------------------------*/
/** @name Constructing and destructing InvestmentFunction
 *  @{ */

 /// constructor of InvestmentFunction, possibly inputting the data
 /** Constructor of InvestmentFunction, taking possibly all the data
  * characterising the function:
  *
  * @param inner_block the only sub-Block of this InvestmentFunction,
  *        representing the problem (B) as stated in the general notes of this
  *        class.
  *
  * @param x an n-vector of pointers to ColVariable representing the x
  *        variable vector in the definition of the function. Note that the
  *        order of the variables in x is crucial, since
  *
  *            THE ORDER OF THE x VECTOR WILL DICTATE THE ORDER OF THE
  *            "ACTIVE" [Col]Variable OF THE InvestmentFunction
  *
  *        That is, get_active_var( 0 ) == x[ 0 ], get_active_var( 1 ) == x[ 1
  *        ], ...
  *
  * @param block_indices a vector containing the indices of the UnitBlocks of
  *        the UCBlock that are subject to investment. The correspondence
  *        between \p block_indices and \p x is positional, i.e., if \p
  *        block_indices is not empty, \p block_indices[ i ] is the index of
  *        the UnitBlock associated with the i-th active variable of this
  *        function (given by get_active_var( i )).
  *
  * @param line_indices a vector containing the indices of the transmission
  *        lines that are subject to investment. The correspondence between \p
  *        line_indices and \p x is positional, i.e., if \p line_indices is
  *        not empty, \p line_indices[ i ] is the index of the transmission
  *        line associated with the active variable of this function whose
  *        index is i + \p block_indices.size(), i.e., given by
  *        get_active_var( i + \p block_indices.size() ).
  *
  * @param linear_coefficients a vector containing the coefficients of the
  *        linear term of this function. The correspondence between \p
  *        linear_coefficients and \p x is positional, i.e., the coefficient
  *        of the i-th active variable of this function (given by
  *        get_active_var( i )) is given by \p linear_coefficients[ i ].
  *
  * @param observer a pointer to the Observer of this InvestmentFunction.
  *
  * As the && implies, \p x, \p block_indices, \p line_indices, and \p
  * linear_coefficients become property of the InvestmentFunction object.
  *
  * All inputs have a default (nullptr, {}, {}, {}, {}, and nullptr,
  * respectively) so that this can be used as the void constructor. */

 InvestmentFunction( Block * inner_block = nullptr , VarVector && x = {} ,
                     IndexVector && block_indices = {} ,
                     IndexVector && line_indices = {} ,
                     RealVector && linear_coefficients = {} ,
                     Observer * const observer = nullptr );

/*--------------------------------------------------------------------------*/
 /// de-serialize an InvestmentFunction out of netCDF::NcGroup
 /** The method takes a netCDF::NcGroup supposedly containing all the
  * *numerical* information required to de-serialize the InvestmentFunction,
  * i.e., the assets to invest (indices of UnitBlocks and transmission lines)
  * and linear coefficients and sets the data of this InvestmentFunction. See
  * the comments to InvestmentFunction::serialize() for the detailed
  * description of the expected format of the netCDF::NcGroup.
  *
  * Note that this method does *not* change the set of active variables, that
  * must be initialized independently either before (like, in the
  * constructor) or after a call to this method (cf. set_variable()).
  *
  * Note, however that there is a significant difference between calling
  * deserialize() before or after set_variables(). More specifically, the
  * difference is between calling the method when the current set of "active"
  * Variable is empty, or not. Indeed, in the former case the number of
  * "active" Variable is dictated by the data found in the netCDF::NcGroup;
  * calling set_variables() afterwards with a vector of different size will
  * fail. Symmetrically, if the set of "active" Variable is not empty when
  * this method is called, finding non-conforming data (number of assets to
  * invest) in the netCDF::NcGroup within this method will cause it to
  * fail. Also, note that in the former case the function is "not completely
  * initialized yet" after deserialize(), and therefore it should not be
  * passed to the Observer quite as yet.
  *
  * Usually [de]serialization is done by Block, but InvestmentFunction is a
  * complex enough object so that having its own ready-made [de]serialization
  * procedure may make sense. Besides, it *is* a Block, and therefore the
  * netCDF::NcGroup will have to contain whatever is managed by the
  * serialize() method of the base Block class in addition to the
  * InvestmentFunction-specific data. However, because this method can then
  * conceivably be called when the InvestmentFunction is attached to an
  * Observer (although it is expected to be used before that), it is also
  * necessary to specify if and how a Modification is issued.
  *
  * @param group a netCDF::NcGroup holding the data in the format described
  *        in the comments to serialize();
  *
  * @param issueMod which decides if and how the FunctionMod (with shift()
  *        == FunctionMod::NaNshift, i.e., "everything changed") is issued,
  *        as described in Observer::make_par(). */

 void deserialize( const netCDF::NcGroup & group , ModParam issueMod );

/*--------------------------------------------------------------------------*/

 /// de-serialize an InvestmentFunction out of netCDF::NcGroup
 /** This method simply calls deserialize( group , eNoMod ). Please refer to
  * the comments to that method for details. The value eNoMod is passed as
  * argument, since this method is mostly thought to be used during
  * initialization when "no one is listening". */

 void deserialize( const netCDF::NcGroup & group ) override {
  deserialize( group , eNoMod );
 }

/*--------------------------------------------------------------------------*/
 /// destructor of InvestmentFunction
 /** Destructor of InvestmentFunction. It destroys the inner Block (if any),
  * releasing its memory. If the inner Block should not be destroyed then,
  * before this InvestmentFunction is destroyed, the pointer to the inner
  * Block must be set to \c nullptr. This can be done by invoking
  * set_inner_block(), passing \c nullptr as a pointer to the new inner Block
  * and \c false to the \c destroy_previous_block parameter. */

 virtual ~InvestmentFunction();

/*--------------------------------------------------------------------------*/
 /// clear method: clears the #v_x vector
 /** Method to "clear" the InvestmentFunction: it clear() the vector
  * #v_x. This destroys the list of "active" Variable without unregistering
  * from them.  Not that the InvestmentFunction would have to, but an Observer
  * using it to "implement itself" should. By not having any Variable, the
  * Observer can no longer do that. */

 void clear( void ) override { v_x.clear(); }

/**@} ----------------------------------------------------------------------*/
/*-------------------------- OTHER INITIALIZATIONS -------------------------*/
/*--------------------------------------------------------------------------*/
/** @name Other initializations
 *  @{ */

 /// sets the set of active Variable of the InvestmentFunction
 /** Sets the set of active Variable of the InvestmentFunction. This method is
  * basically provided to work in tandem with the methods which only load the
  * "numerical data" of the InvestmentFunction, for instance,
  * deserialize(). These (if the variables have not been defined prior to
  * calling them, see below) leave the InvestmentFunction in a somewhat
  * inconsistent state whereby one knows the data but not the input Variable,
  * cue this method.
  *
  * Note that there are two distinct patterns of usage:
  *
  * - set_variables() is called *before* deserialize();
  *
  * - set_variables() is called *after* deserialize().
  *
  * In the former case, after the call to set_variables(), the
  * InvestmentFunction has active variables, but data necessary for its
  * computation is not present. In the latter case, the data is there, except
  * that the InvestmentFunction has no input Variable; the object is in a
  * not-fully-consistent defined state. Having an Observer (then, Solver)
  * dealing with a call to set_variables() would be possible by issuing a
  * FunctionModVars( ... , AddVar ), but this is avoided because this method
  * is only thought to be called during initialization where the Observer is
  * not there already, whence no "issueMod" parameter.
  *
  * @param x a n-vector of pointers to ColVariable representing the x variable
  *        vector in the definition of the function. Note that the order of
  *        the variables in x is crucial, since the correspondence with the
  *        indices of the assets and the linear coefficients (whether already
  *        provided, or to be provided later, cf. discussion above) is
  *        positional. The i-th linear coefficient of this InvestmentFunction
  *        is the coefficient of the i-th active variable in the linear term
  *        of this function. Moreover, by letting "NumUnitBlocks" and
  *        "NumLines" the number of UnitBlocks and transmission lines to
  *        invest:
  *
  *          - For each i in {0, ..., NumUnitBlocks-1}, the i-th active
  *            Variable of this InvestmentFunction represents the investment
  *            to be made on the i-th UnitBlock (among the UnitBlock that are
  *            subject to investment).
  *
  *          - For each i in {0, ..., NumLines-1}, the (NumUnitBlocks + i)-th
  *            active Variable of this InvestmentFunction represents the
  *            investment to be made on the i-th line (among the lines that
  *            are subject to investment).
  *
  *        After the call to this method, <tt>x[ 0 ] == get_active_var( 0 ),
  *        x[ 1 ] = get_active_var( 1 )</tt>, ...
  *
  * As the && implies, x become property of the InvestmentFunction object. */

 void set_variables( VarVector && x );

/*--------------------------------------------------------------------------*/
 /// set the (only) sub-Block of the InvestmentFunction
 /** This method sets the only sub-Block of the InvestmentFunction
  * (a.k.a. Block B representing problem (B) in the definition of this
  * InvestmentFunction).
  *
  * @param block the pointer to a Block satisfying the conditions stated in
  *        the definition of this InvestmentFunction.
  *
  * @param destroy_previous_block indicates whether the previous inner Block
  *        must be destroyed. The default value of this parameter is \c true,
  *        which means that the previous inner Block (if any) is destroyed and
  *        its allocated memory is released. */

 void set_inner_block( Block * block , bool destroy_previous_block = true ) {
  if( ( ! v_Block.empty() ) && block == v_Block.front() &&
      ( ! destroy_previous_block ) )
   return; // the given Block is already here; silently return

  if( destroy_previous_block && ! v_Block.empty() )
   delete v_Block.front();

  v_Block.clear();
  v_Block.push_back( block );

  if( block )
   block->set_f_Block( this );

  send_nuclear_modification();
 }

/**@} ----------------------------------------------------------------------*/
/*----------------- METHODS FOR MANAGING THE "IDENTITY" --------------------*/
/*--------------------------------------------------------------------------*/
/** @name Managing the "identity" of the InvestmentFunction
 *
 * Actually implement the methods of ThinComputeInterface relative to
 * temporarily changing the "identity" of the InvestmentFunction.
 *
 *  @{ */

 /// set the "identity" of the InvestmentFunction
 /** Actually implement the ThinComputeInterface::set_id() by setting the f_id
  * member of the InvestmentFunction class, which is initialized with "this"
  * in the constructor. InvestmentFunction always uses f_id to try to "lock
  * and own" the Block. This method allows to change f_id ("lend another
  * identity"); when called with nullptr argument, the id() is reset to the
  * default "this". Also, if the inner Block is set and has registered Solver,
  * their identity is also set (or reset) in anticipation that they also may
  * have to lock() the inner Block during their line of work. */

 void set_id( void * id = nullptr ) override {
  if( f_id == id )  // nothing to do
   return;          // silently (and cowardly) return

  f_id = id ? id : this;

  // propagate downwards the id change
  if( ! v_Block.empty() )
   for( auto s : v_Block.front()->get_registered_solvers() )
    s->set_id( id );
 }

/**@} ----------------------------------------------------------------------*/
/*---- METHODS FOR HANDLING "ACTIVE" Variable IN THE InvestmentFunction ----*/
/*--------------------------------------------------------------------------*/
/** @name Methods for handling the set of "active" Variable in the
 * InvestmentFunction; this is the actual concrete implementation exploiting
 * the vector v_x of pointers.
 * @{ */

 Index get_num_active_var( void ) const override final {
  return( v_x.size() );
 }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

 Index is_active( const Variable * const var ) const override final
 {
  auto idx = std::find( v_x.begin() , v_x.end() , var );
  if( idx == v_x.end() )
   return( Inf<Index>() );
  else
   return( std::distance( v_x.begin() , idx ) );
 }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

 void map_active( c_Vec_p_Var & vars , Subset & map , bool ordered = false )
  const override final;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

 Variable * get_active_var( Index i ) const override final {
  return( v_x[ i ] );
 }

/*--------------------------------------------------------------------------*/

 v_iterator * v_begin( void ) override final {
  return( new InvestmentFunction::v_iterator( v_x.begin() ) );
 }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

 v_const_iterator * v_begin( void ) const override final {
  return( new InvestmentFunction::v_const_iterator( v_x.begin() ) );
 }

/*--------------------------------------------------------------------------*/

 v_iterator * v_end( void ) override final {
  return( new InvestmentFunction::v_iterator( v_x.end() ) );
 }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

 v_const_iterator * v_end( void ) const override final {
  return( new InvestmentFunction::v_const_iterator( v_x.end() ) );
 }

/**@} ----------------------------------------------------------------------*/
/*------------- METHODS FOR MODIFYING THE InvestmentFunction ---------------*/
/*--------------------------------------------------------------------------*/
/** @name Methods for modifying the InvestmentFunction
 *  @{ */

/*--------------------------------------------------------------------------*/
 /// remove the i-th active Variable
 /** This method removes the active Variable whose index is \p i.
  *
  * @param i the index of the Variable to be removed. It must be an integer
  *        between 0 and get_num_active_var() - 1.
  *
  * @param issueMod decides if and how the C05FunctionModVarsRngd (since a
  *        InvestmentFunction is strongly quasi-additive, and with shift() == 0
  *        as expected) is issued, as described in Observer::make_par(). */

 void remove_variable( Index i , ModParam issueMod = eModBlck )
  override final;

/*--------------------------------------------------------------------------*/
 /// remove a range of active Variable
 /** This method removes a range of "active" Variable.
  *
  * @param range contains the indices of the Variable to be deleted
  *        (hence, range.second <= get_num_active_var());
  *
  * @param issueMod decides if and how the C05FunctionModVarsRngd (since a
  *        InvestmentFunction is strongly quasi-additive, and with shift() == 0
  *        as expected) is issued, as described in Observer::make_par(). */

 void remove_variables( Range range , ModParam issueMod = eModBlck )
  override final;

/*--------------------------------------------------------------------------*/
 /// remove a subset of Variable
 /** This method removes all the Variable in the given set of indices. If \p
  * indices is empty, all Variable are removed.
  *
  * @param indices a Subset & containing the indices of the Variable to be
  *        removed, i.e., integers between 0 and get_num_active_var() - 1. If
  *        \p indices is empty, all Variable are removed.
  *
  * @param ordered a bool indicating if \p indices is already ordered in
  *        increasing sense (otherwise this is done inside the method, which
  *        is why \p indices is not const).
  *
  * @param issueMod decides if and how the C05FunctionModVars (with f_shift ==
  *        0, since an InvestmentFunction is strongly quasi-additive) is
  *        issued, as described in Observer::make_par(). */

 void remove_variables( Subset && indices , bool ordered = false ,
                        ModParam issueMod = eModBlck ) override final;

/** @} ---------------------------------------------------------------------*/
/*----------- METHODS FOR Saving THE DATA OF THE InvestmentFunction --------*/
/*--------------------------------------------------------------------------*/
/** @name Saving the data of the InvestmentFunction
 *  @{ */

 /// serialize an InvestmentFunction into a netCDF::NcGroup
 /** Serialize an InvestmentFunction into a netCDF::NcGroup. Note that,
  * InvestmentFunction being both a Function and a Block, the netCDF::NcGroup
  * will have to have the "standard format of a :Block", meaning whatever is
  * managed by the serialize() method of the base Block class, plus the
  * InvestmentFunction-specific data with the following format:
  *
  * - The dimension "NumUnitBlocks" containing the number of UnitBlock that
  *   are subject to investment. This dimension is optional. If it is not
  *   provided, then it is assumed that NumUnitBlocks = 0.
  *
  * - The dimension "NumLines" containing the number of lines that are subject
  *   to investment. This dimension is optional. If it is not provided, then
  *   it is assumed that NumLines = 0.
  *
  * The sum of the dimensions "NumUnitBlocks" and "NumLines", let us call it
  * "NumVar", provides the number of active Variable of this
  * InvestmentFunction. For each i in {0, ..., NumUnitBlocks-1}, the i-th
  * active Variable of this InvestmentFunction represents the investment to be
  * made on the i-th UnitBlock (among the UnitBlock that are subject to
  * investment). For each i in {0, ..., NumLines-1}, the (NumUnitBlocks +
  * i)-th active Variable of this InvestmentFunction represents the investment
  * to be made on the i-th line (among the lines that are subject to
  * investment).
  *
  * - The one-dimensional variable "BlockIndices", of type netCDF::Uint() and
  *   indexed over "NumUnitBlocks", containing the indices of the UnitBlock of
  *   the UCBlock that are subject to investment. The i-th active Variable of
  *   this InvestmentFunction will represent the investment (e.g., scale
  *   factor) to be made on the UnitBlock of the UCBlock whose index is
  *   BlockIndices[ i ].
  *
  * - The one-dimensional variable "LineIndices", of type netCDF::Uint() and
  *   indexed over "NumLines", containing the indices of the lines that are
  *   subject to investment. The (NumUnitBlocks + i)-th active Variable of
  *   this InvestmentFunction will represent the investment to be made on the
  *   line whose index is LineIndices[ i ].
  *
  * - The one-dimensional variable "LinearCoefficients", of type
  *   netCDF::NcDouble(), which is either a scalar or indexed over "NumVar",
  *   containing the coefficients of the linear term of the function
  *   represented by this InvestmentFunction. If it is a scalar, then we
  *   assume that LinearCoefficients[i] = LinearCoefficients[0] for all i in
  *   {0, ..., NumVar - 1}. The i-th element of this vector is the coefficient
  *   of the i-th active Variable of this InvestmentFunction. This variable is
  *   optional. If it is not provided then all coefficients are considered to
  *   be zero.
  *
  * - The group "SDDPBlock", containing the description of the inner Block. */

 void serialize( netCDF::NcGroup & group ) const override;

/**@} ----------------------------------------------------------------------*/
/*------- METHODS DESCRIBING THE BEHAVIOR OF THE InvestmentFunction --------*/
/*--------------------------------------------------------------------------*/
/** @name Methods describing the behavior of the InvestmentFunction
 *  @{ */

 /// compute the InvestmentFunction

 int compute( bool changedvars = true ) override;

/*--------------------------------------------------------------------------*/
 /// returns the value of the InvestmentFunction
 /** This method returns an approximation to the value of this
  * InvestmentFunction associated with the most recent call to compute(). The
  * returned value depends on the sense of the Objective of the sub-Block. If
  * the sense of the Objective of the sub-Block is "minimization", then this
  * method returns a valid upper bound on the optimal objective function value
  * of the sub-Block (see Solver::get_ub()). If the sense of the Objective of
  * the sub-Block is "maximization", then this method returns a valid upper
  * bound on the optimal objective function value of the sub-Block (see
  * Solver::get_lb()).
  *
  * Notice that if compute() has never been invoked, then the value returned
  * by this method is meaningless. Moreover, if this InvestmentFunction does
  * not have a sub-Block or its sub-Block does not have a Solver attached to
  * it, then an exception is thrown. */

 FunctionValue get_value( void ) const override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
 /// returns a lower estimate of the InvestmentFunction
 /** This method simply returns get_value().  */

 FunctionValue get_lower_estimate( void ) const override {
  return( get_value() );
 }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
 /// returns an upper estimate of the InvestmentFunction
 /** This method simply returns get_value().  */

 FunctionValue get_upper_estimate( void ) const override {
  return( get_value() );
 }

/*--------------------------------------------------------------------------*/
 /// returns the "constant term" of the InvestmentFunction

 FunctionValue get_constant_term( void ) const override;

/*--------------------------------------------------------------------------*/
 /// returns true only if this InvestmentFunction is convex
 /** This method returns true only if this InvestmentFunction is convex. If
  * this InvestmentFunction has no sub-Block or the sense of the Objective of
  * its sub-Block is maximization, then this method returns false. Otherwise,
  * it returns true. */

 bool is_convex( void ) const override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
 /// returns true only if this InvestmentFunction is concave
 /** This method returns true only if this InvestmentFunction is concave. If
  * this InvestmentFunction has no sub-Block or the sense of the Objective of
  * its sub-Block is minimization, then this method returns false. Otherwise,
  * it returns true. */

 bool is_concave( void ) const override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
 /// returns true only if this InvestmentFunction is linear
 /** Method that returns true only if this InvestmentFunction is linear. In
  * particular (and probably very rare) cases, this Function could be
  * linear. We do not attempt to find this out and this method simply returns
  * \c false. */

 bool is_linear( void ) const override { return( false ); }

/*--------------------------------------------------------------------------*/
 /// tells whether a linearization is available

 bool has_linearization( bool diagonal = true ) override final;

/*--------------------------------------------------------------------------*/

 void get_linearization_coefficients
 ( FunctionValue * g , Range range = std::make_pair( 0 , Inf<Index>() ) ,
   Index name = Inf<Index>() ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

 void get_linearization_coefficients
 ( SparseVector & g , Range range = std::make_pair( 0 , Inf<Index>() ) ,
   Index name = Inf<Index>() ) override;

/*--------------------------------------------------------------------------*/

 void get_linearization_coefficients
 ( FunctionValue * g , c_Subset & subset  , bool ordered = false ,
   Index name = Inf<Index>() ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

 void get_linearization_coefficients
 ( SparseVector & g , c_Subset & subset , bool ordered = false ,
   Index name = Inf<Index>() ) override;

/*--------------------------------------------------------------------------*/
 /// return the constant term of a linearization

 FunctionValue get_linearization_constant( Index name = Inf<Index>() )
  override final;

/*--------------------------------------------------------------------------*/
 /// return a pointer to the (only) sub-Block of the InvestmentFunction
 /** This method returns a pointer to the only sub-Block of the
  * InvestmentFunction (a.k.a. Block B representing problem (B) in the
  * definition of this InvestmentFunction). If this InvestmentFunction has no
  * sub-Block, a \c nullptr is returned. */

 Block * get_inner_block( void ) const {
  if( v_Block.empty() )
   return( nullptr );
  return( v_Block.front() );
 }

/*--------------------------------------------------------------------------*/
 /// returns a pointer to the Solver attached to the sub-Block (if any)
 /** This method returns a pointer to the Solver attached to the sub-Block of
  * this InvestmentFunction. The template parameter \p T indicates the type of
  * Solver whose pointer will be returned; its default value is Solver. If
  *
  * - this InvestmentFunction does not have a sub-Block; or
  *
  * - the sub-Block of this InvestmentFunction does not have a Solver attached
  *   to it; or
  *
  * - the Solver attached to the sub-Block is not or does not derive from \p T
  *
  * then a nullptr is returned. Otherwise, a pointer of type \p T is
  * returned. */

 template<class T = Solver>
 inline T * get_solver() const {
  if( v_Block.empty() )
   return nullptr;

  if( v_Block.front()->get_registered_solvers().empty() )
   return nullptr;

  return dynamic_cast< T * >
   ( v_Block.front()->get_registered_solvers().front() );
 }

/**@} ----------------------------------------------------------------------*/
/*-------------------- PROTECTED PART OF THE CLASS -------------------------*/
/*--------------------------------------------------------------------------*/

 protected:

/*--------------------------------------------------------------------------*/
/*--------------------------- PROTECTED METHODS ----------------------------*/
/*--------------------------------------------------------------------------*/
/** @name Protected methods for printing
    @{ */

 /// print information about the InvestmentFunction on an ostream
 /** Protected method intended to print information about the
  * InvestmentFunction; it is virtual so that derived classes can print their
  * specific information in the format they choose. */

 void print( std::ostream &output ) const override {
  output << "InvestmentFunction [" << this << "]"
         << " with " << get_num_active_var() << " active variables";
 }

 /// load the InvestmentFunction out of an input stream
 /** This method loads the InvestmentFunction out of an input stream. */

 void load( std::istream &input ) override final;

/**@} ----------------------------------------------------------------------*/
/*--------------------------- PROTECTED FIELDS  ----------------------------*/
/*--------------------------------------------------------------------------*/

 VarVector v_x; ///< the pointers to the active variables x

 bool f_blocks_are_updated = false;
 ///< indicates whether the sub-Blocks are updated

 int f_solver_status = 0;
 ///< the most recent status returned by the Solver of the sub-Block

 bool f_diagonal_linearization_required = false;
 ///< indicates whether a diagonal linearization is required

 bool f_ignore_modifications = false; ///< ignore any Modification

 void * f_id; ///< the "identity" of the InvestmentFunction

 double f_value;
 ///< the value of this InvestmentFunction after compute() is called

 std::vector< Index > v_block_indices;
 ///< indices of the UnitBlocks that are subject to investment

 std::vector< Index > v_line_indices;
 ///< indices of the lines that are subject to investment

 std::vector< double > v_linearization;
 ///< linearization associated with the most recent call to compute()

 std::vector< double > v_linear_coefficients;
 ///< linear coefficients of the active Variable

 std::vector< std::vector< std::vector< Index > > > generator_node_map;
 ///< maps the index of a generator to the node it belongs to
 /**< This vector maps a generator to the node it belongs to. The generator is
  * identified by a triplet (stage, unit_block_index, generator_index), where
  * stage indicates the stage (between 0 and SDDPBlock::get_time_horizon() -
  * 1), unit_block_index identifies the UnitBlock to which the generator
  * belongs (and index between 0 and v_block_indices.size() - 1) and
  * generator_index is the index of the generator within its UnitBlock. */

/*--------------------------------------------------------------------------*/
/*--------------------- PRIVATE PART OF THE CLASS --------------------------*/
/*--------------------------------------------------------------------------*/

 private:

/*--------------------------------------------------------------------------*/
/*-------------------------- PRIVATE METHODS -------------------------------*/
/*--------------------------------------------------------------------------*/

 /// update the sub-Block of the UCBlock
 /** This function updates the sub-Block of the UCBlock to reflect the current
  * values of the x variables. */

 void update_blocks();

/*--------------------------------------------------------------------------*/

 /// sends a nuclear modification, invalidates the global pool
 /** Besides sending a "nuclear modification" for Function, it also invalidates
  * the global pool and declares that the Constraint of the sub-Block are not
  * updated.
  *
  * @param chnl the name of the channel to which the Modification should be
  *        sent. */

 void send_nuclear_modification( const Observer::ChnlName chnl = 0 );

/*--------------------------------------------------------------------------*/

 /// return a pointer to the Solver of the UCBlock associated with \p stage
 /** This function returns a pointer to the Solver of the UCBlock associated
  * with the given \p stage.
  *
  * @param stage A stage.
  *
  * @return A pointer to the Solver of the UCBlock associated with the given
  *         \p stage. */

 CDASolver * get_ucblock_solver( Index stage ) const;

/*--------------------------------------------------------------------------*/

 UCBlock * get_ucblock( Index stage ) const;

/*--------------------------------------------------------------------------*/

 /// reset the BlockConfig of the inner Block to the default one
 void set_default_inner_Block_BlockConfig();

/*--------------------------------------------------------------------------*/

 /// reset the BlockSolverConfig of the inner Block to the default one
 void set_default_inner_Block_BlockSolverConfig();

/*--------------------------------------------------------------------------*/

 /// reset the configuration of the inner Block to the default one
 /** Reset both the BlockConfig and the BlockSolverConfig of the inner Block
  * to the default ones. */

 void set_default_inner_Block_configuration() {
  set_default_inner_Block_BlockSolverConfig();
  set_default_inner_Block_BlockConfig();
 }

/*--------------------------------------------------------------------------*/

 int get_inner_block_objective_sense() const;

/*--------------------------------------------------------------------------*/

 /// prepares the linearization for a new simulation
 void reset_linearization();

/*--------------------------------------------------------------------------*/

 double compute_scale_linearization( Index i , Index stage );

/*--------------------------------------------------------------------------*/

 double compute_kappa_linearization
 ( const IntermittentUnitBlock * intermittent_unit );

/*--------------------------------------------------------------------------*/

 /// updates the linearization to reflect the most recent scenario considered
 void update_linearization();

/*--------------------------------------------------------------------------*/

 /// updates the linearization with respect to the set of UnitBlock
 void update_linearization_unit_blocks( Index stage );

/*--------------------------------------------------------------------------*/

 /// updates the linearization with respect to the set of NetworkBlock
 void update_linearization_network_blocks( Index stage );

/*--------------------------------------------------------------------------*/

 /// returns the node to which the given generator belongs
 Index get_node( Index stage , Index unit_block_index , Index generator ) const;

/*--------------------------------------------------------------------------*/

 /// builds the mapping between generator and the node it belongs to
 void build_generator_node_map();

/*--------------------------------------------------------------------------*/

 /// returns a pointer to the BendersBFunction associated with the given stage
 BendersBFunction * get_benders_function( Index stage ) const;

/*--------------------------------------------------------------------------*/

 SMSpp_insert_in_factory_h; // insert InvestmentFunction in the Block factory

/*--------------------------------------------------------------------------*/
/*---------------------------- PRIVATE FIELDS  -----------------------------*/
/*--------------------------------------------------------------------------*/

 /// Name of the netCDF sub-group containing the description of the inner Block
 inline static const std::string BLOCK_NAME = "SDDPBlock";

};  // end( class( InvestmentFunction ) )

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

/** @} end( group( InvestmentFun_CLASSES ) ) -------------------------------*/
/*--------------------------------------------------------------------------*/

}  // end( namespace SMSpp_di_unipi_it )

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

#endif  /* InvestmentFunction.h included */

/*--------------------------------------------------------------------------*/
/*------------------- End File InvestmentFunction.h ------------------------*/
/*--------------------------------------------------------------------------*/
