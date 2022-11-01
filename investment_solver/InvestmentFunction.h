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
 class BatteryUnitBlock;       // forward declaration of BatteryUnitBlock

 class BendersBFunction;       // forward declaration of BendersBFunction

 class IntermittentUnitBlock;  // forward declaration of IntermittentUnitBlock

 class SDDPBlock;              // forward declaration of SDDPBlock

 class UCBlock;                // forward declaration of UClock

 class UnitBlock;              // forward declaration of UnitBlock

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

 /// public enum representing the types of assets
 /** Public enum representing the types of assets. */

 enum AssetType { eUnitBlock = 0 , eLine = 1 };

 /// public enum representing the sides of the linear constraints
 /** Public enum representing the sides of the linear constraints. */

 enum ConstraintSide { eLHS = 0 , eRHS = 1 };

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
 using MultiVector = std::vector< RealVector >;
 using AssetTypeVector = std::vector< AssetType >;

 using VarVector = std::vector< ColVariable * >;
 ///< representing the x variables upon which the function depends

 using ViolatedConstraint = std::pair< Index , ConstraintSide >;

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

/*--------------------------------------------------------------------------*/
 /// public enum for the int algorithmic parameters
 /** Public enum describing the different algorithmic parameters of int type
  * that InvestmentFunction has in addition to those of C05Function. The value
  * intLastInvestmentFPar is provided so that the list can be easily further
  * extended by derived classes. */

 enum int_par_type_InvestmentF {

  intComputeLinearization = intLastParC05F ,
  ///< determines whether linearizations must be computed

  intOutputSolution ,
  ///< indicates whether the solution should be output
  /**< If the value for this parameter is nonzero, then part of the primal and
   * dual solutions obtained for each UCBlock for each scenario is output
   * while this InvestmentFunction is being compute()-ed. The default value of
   * this parameter is 0, which means that no solution is output. */

  intLastInvestmentFPar
  ///< first allowed new int parameter for derived classes
  /**< Convenience value for easily allow derived classes to extend the set of
   * int algorithmic parameters. */

 };  // end( int_par_type_InvestmentF )

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
  * @param asset_indices a vector containing the indices of the assets that
  *        are subject to investment. An asset can be either a UnitBlock or a
  *        transmission line. If it is a UnitBlock, then its index is simply
  *        the index that this UnitBlock has within its UCBlock. If it is a
  *        transmission line, then its index is the index that this
  *        transmission line has within its NetworkBlock. The correspondence
  *        between \p asset_indices and \p x is positional, i.e., if \p
  *        asset_indices is not empty, \p asset_indices[ i ] is the index of
  *        the asset associated with the i-th active variable of this function
  *        (given by get_active_var( i )).
  *
  * @param asset_type a vector containing the type of each asset.

  * @param cost a vector containing the coefficients of the linear term of
  *        this function associated with investments. The correspondence
  *        between \p cost and \p x is positional, i.e., the coefficient of
  *        the i-th active variable of this function (given by get_active_var(
  *        i )) is given by \p cost[ i ].
  *
  * @param disinvestment_cost a vector containing the coefficients of the
  *        linear term of this function associated with disinvestments. The
  *        correspondence between \p disinvestment_cost and \p x is
  *        positional, i.e., the coefficient of the i-th active variable of
  *        this function (given by get_active_var( i )) is given by \p
  *        disinvestment_cost[ i ].
  *
  * @param observer a pointer to the Observer of this InvestmentFunction.
  *
  * As the && implies, \p x, \p block_indices, \p line_indices, \p cost, and
  * \p disinvestment_cost become property of the InvestmentFunction object.
  *
  * All inputs have a default (nullptr, {}, {}, {}, {}, and nullptr,
  * respectively) so that this can be used as the void constructor. */

 InvestmentFunction( Block * inner_block = nullptr , VarVector && x = {} ,
                     IndexVector && asset_indices = {} ,
                     AssetTypeVector && asset_type = {} ,
                     RealVector && cost = {} ,
                     RealVector && disinvestment_cost = {} ,
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

 void clear() override { v_x.clear(); }

/**@} ----------------------------------------------------------------------*/
/*-------------------------- OTHER INITIALIZATIONS -------------------------*/
/*--------------------------------------------------------------------------*/
/** @name Other initializations
 *  @{ */

 /// set the whole set of parameters of this InvestmentFunction
 /** The extra Configuration of the given ComputeConfig (see
  * ComputeConfig::f_extra_Configuration), if not nullptr, is assumed to be of
  * type SimpleConfiguration< std::map< std::string , Configuration * > >. If
  * it is not of this type, an exception is thrown. The map in that
  * SimpleConfiguration is meant to provide pointers to a number of
  * Configuration, that should be used in different situations. The following
  * keys, and their corresponding Configuration, are considered:
  *
  * - "BlockConfig": a pointer to a BlockConfig to be applied to the inner
  *    Block of this BendersBFunction.
  *
  * - "BlockSolverConfig": a pointer to a BlockSolverConfig to be applied to
  *    the inner Block of this BendersBFunction.
  *
  * If a key that is not any of the above is provided, an exception is
  * thrown. No pointer is kept by the InvestmentFunction, so the caller can
  * (and is responsible to) delete any pointer provided.
  *
  * If the given pointer to the ComputeConfig is nullptr, then the
  * Configuration of the InvestmentFunction is reset to its default. This
  * means that
  *
  *  (1) all parameters of the InvestmentFunction are reset to their default
  *      values;
  *
  *  (2) the inner Block (if any) is configured to its default configuration;
  *
  *  (3) the Solver of the inner Block (and their sub-Block, recursively) are
  *      unregistered and deleted.
  *
  * If the given pointer to the ComputeConfig is not nullptr but its extra
  * Configuration is nullptr, then (2) and (3) above are performed. If the
  * pointer to the BlockConfig (if provided) in the extra Configuration is
  * nullptr, then (2) above is performed. If the pointer to the
  * BlockSolverConfig (if provided) in the extra Configuration is nullptr,
  * then (3) above is performed.
  *
  * @param scfg a pointer to a ComputeConfig. */

 void set_ComputeConfig( ComputeConfig *scfg = nullptr ) override;

/*--------------------------------------------------------------------------*/

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
  if( ( v_Block.size() == 1 ) && ( block == v_Block.front() ) &&
      ( ! destroy_previous_block ) )
   return; // the given Block is already here; silently return

  if( destroy_previous_block )
   for( auto block : v_Block )
    delete block;

  v_Block.clear();
  v_Block.push_back( block );

  if( block )
   block->set_f_Block( this );

  send_nuclear_modification();
 }

/*--------------------------------------------------------------------------*/
 /// set the sub-Block of the InvestmentFunction
 /** This method sets the sub-Block of the InvestmentFunction (a.k.a. Block B
  * representing problem (B) in the definition of this InvestmentFunction).
  *
  * @param blocks the pointers to the (identical) Block satisfying the
  *        conditions stated in the definition of this InvestmentFunction.
  *
  * @param destroy_previous_blocks indicates whether the previous inner Block
  *        must be destroyed. The default value of this parameter is \c true,
  *        which means that the previous inner Block (if any) are destroyed
  *        and their allocated memory is released. */

 void set_inner_blocks( std::vector< Block * > & blocks ,
                        bool destroy_previous_blocks = true ) {
  if( ( ! v_Block.empty() ) && ( blocks.size() == v_Block.size() ) &&
      ( ! destroy_previous_blocks ) ) {

   bool blocks_are_here = true;
   for( Index i = 0 ; i < blocks.size() ; ++i )
    if( blocks[ i ] != v_Block[ i ] ) {
     blocks_are_here = false;
     break;
    }

   if( blocks_are_here )
    return; // the given Blocks are already here; silently return
  }

  if( destroy_previous_blocks )
   for( auto block : v_Block )
    delete block;

  v_Block.clear();
  v_Block = blocks;

  for( auto block : v_Block )
   if( block )
    block->set_f_Block( this );

  send_nuclear_modification();
 }

/*--------------------------------------------------------------------------*/
 /// set a given integer (int) numerical parameter
 /** Set a given integer (int) numerical parameter. InvestmentFunctiontakes
  * care of the following parameters:
  *
  * - intGPMaxSz: This parameter specifies the maximum number of
  *               linearizations that can be stored in the global pool. The
  *               default value for this parameter is defined by the
  *               C05Function.
  *
  * - intComputeLinearization: This parameter indicates whether linearizations
  *                            must be computed. The default value is 1, which
  *                            means that linearizations are computed.
  *
  * - intOutputSolution: This parameter indicates whether the solution of each
  *                      UCBlock must be output. If the value for this
  *                      parameter is nonzero, then part of the primal and
  *                      dual solutions obtained for each UCBlock for each
  *                      scenario is output while this InvestmentFunction is
  *                      being compute()-ed. The default value of this
  *                      parameter is 0, which means that no solution is
  *                      output.
  *
  * Any other parameter is handled by the C05Function.
  *
  * @param par The parameter to be set.
  *
  * @return The value of the parameter. */

 void set_par( idx_type par , int value ) override;

/*--------------------------------------------------------------------------*/
 /// set a given float (double) numerical parameter
 /** Set a given float (double) numerical parameter. InvestmentFunction takes
  * care of the following parameters. Any other parameter is handled by the
  * C05Function.
  *
  * - dblAAccMlt
  *
  * @param par The parameter to be set.
  *
  * @return The value of the parameter. */

 void set_par( idx_type par , double value ) override {

  switch( par ) {
   case( dblAAccMlt ):
    AAccMlt = value;
    break;
   default: C05Function::set_par( par , value );
  }
 }

/** @} ---------------------------------------------------------------------*/
/*------------------- METHODS FOR HANDLING THE PARAMETERS ------------------*/
/*--------------------------------------------------------------------------*/
/** @name Handling the parameters of the InvestmentFunction
 *  @{ */

 [[nodiscard]] idx_type get_num_int_par() const override {
  return( intLastInvestmentFPar );
 }

/*--------------------------------------------------------------------------*/

 /// get a specific integer (int) numerical parameter
 /** Get a specific integer (int) numerical parameter. InvestmentFunction
  * takes care of the following parameters:
  *
  * - intGPMaxSz
  *
  * - intComputeLinearization
  *
  * - intOutputSolution
  *
  * Any other parameter is handled by the C05Function.
  *
  * @param par The parameter whose value is desired.
  *
  * @return The value of the required parameter. */

 [[nodiscard]] int get_int_par( idx_type par ) const override {
  switch( par ) {
   case( intGPMaxSz ): return( global_pool.size() );
   case( intComputeLinearization ): return( f_compute_linearization );
   case( intOutputSolution ): return( f_output_solution );
  }
  return( C05Function::get_int_par( par ) );
 }

/*--------------------------------------------------------------------------*/
 /// get a specific float (double) numerical parameter
 /** Get a specific float (double) numerical parameter. InvestmentFunction
  * takes care of the following parameters:
  *
  * - dblAAccMlt
  *
  * Any other parameter is handled by the C05Function.
  *
  * @param par The parameter whose value is desired.
  *
  * @return The value of the required parameter. */

 [[nodiscard]] double get_dbl_par( idx_type par ) const override {
  switch( par ) {
   case( dblAAccMlt ): return( AAccMlt );
  }

  return( C05Function::get_dbl_par( par ) );
 }

/*--------------------------------------------------------------------------*/

 [[nodiscard]] int get_dflt_int_par( idx_type par ) const override {
  if( par == intComputeLinearization )
   return( 1 );
  if( par == intOutputSolution )
   return( 0 );
  return( C05Function::get_dflt_int_par( par ) );
 }

/*--------------------------------------------------------------------------*/

 [[nodiscard]] idx_type int_par_str2idx( const std::string & name )
  const override {
  if( name == "intComputeLinearization" )
   return( intComputeLinearization );
  if( name == "intOutputSolution" )
   return( intOutputSolution );
  return( C05Function::int_par_str2idx( name ) );
 }

/*--------------------------------------------------------------------------*/

 [[nodiscard]] const std::string & int_par_idx2str( idx_type idx )
  const override {
  static const std::vector< std::string > pars = { "intComputeLinearization" ,
                                                   "intOutputSolution" };
  if( ( idx >= intComputeLinearization ) && ( idx < intLastInvestmentFPar ) )
   return( pars[ idx - intComputeLinearization ] );
  return( C05Function::int_par_idx2str( idx ) );
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
  for( auto block : v_Block )
   for( auto s : block->get_registered_solvers() )
    s->set_id( id );
 }

/**@} ----------------------------------------------------------------------*/
/*---- METHODS FOR HANDLING "ACTIVE" Variable IN THE InvestmentFunction ----*/
/*--------------------------------------------------------------------------*/
/** @name Methods for handling the set of "active" Variable in the
 * InvestmentFunction; this is the actual concrete implementation exploiting
 * the vector v_x of pointers.
 * @{ */

 Index get_num_active_var() const override final {
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

 v_iterator * v_begin() override final {
  return( new InvestmentFunction::v_iterator( v_x.begin() ) );
 }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

 v_const_iterator * v_begin() const override final {
  return( new InvestmentFunction::v_const_iterator( v_x.begin() ) );
 }

/*--------------------------------------------------------------------------*/

 v_iterator * v_end() override final {
  return( new InvestmentFunction::v_iterator( v_x.end() ) );
 }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

 v_const_iterator * v_end() const override final {
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

/*--------------------------------------------------------------------------*/

 /// it informs the InvestmentFunction whether the bounds were reformulated
 /** This functions informs the InvestmentFunction whether the bounds on the
  * active variables have been reformulated.
  *
  * The active variables of this InvestmentFunction may have "natural" lower
  * and upper bounds in the model in which they are defined. That is, for each
  * active variable x, there may be l and u such that x must satisfy
  *
  *     l <= x <= u.
  *
  * These natural bounds, however, may have been reformulated such that x
  * becomes a nonnegative variable satisfying
  *
  *     0 <= x <= u - l
  *
  * if the lower bound l is finite. This function informs the
  * InvestmentFunction whether the natural bounds on the active variables have
  * been reformulated.
  *
  * @param reformulated If true, this means that the bounds on the active
  *        variables have been reformulated. Otherwise, the bounds are the
  *        "natural" ones. */

 void reformulated_bounds( bool reformulated ) {
  f_reformulated_bounds = reformulated;
 }

/*--------------------------------------------------------------------------*/

 void set_number_sub_blocks( Index n ) {
  assert( v_Block.empty() ||
          std::all_of( v_Block.cbegin() , v_Block.cend() ,
                       []( Block * b ) { return b == nullptr ; } ) );
  f_num_sub_blocks = n;
 }

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
  * - The attribute "ReplicateBatteryUnits", of type netCDF::Int(), which
  *   indicates that investment in a battery unit is made by replicating the
  *   unit, i.e., by considering multiple identical units of that unit. This
  *   attribute is optional. If it is not provided or its value is zero, then
  *   investing in a battery unit means scaling its minimum and maximum power
  *   and storage levels. Otherwise, the battery units are replicated.
  *
  * - The attribute "ReplicateIntermittentUnits", of type netCDF::Int(), which
  *   indicates that investment in an intermittent unit is made by replicating
  *   the unit, i.e., by considering multiple identical units of that
  *   unit. This attribute is optional. If it is not provided or its value is
  *   zero, then investing in an intermittent unit means scaling its minimum
  *   and maximum power. Otherwise, the intermittent units are replicated.
  *
  * - The dimension "NumAssets" containing the number of assets that are
  *   subject to investment. This dimension is optional. If it is not
  *   provided, then it is assumed that NumAssets = 0.
  *
  * - The one-dimensional variable "Assets", of type netCDF::Uint() and
  *   indexed over "NumAssets", containing the indices of the assets that are
  *   subject to investment. An asset can be either a UnitBlock or a
  *   transmission line. If it is a UnitBlock, then its index is simply the
  *   index that this UnitBlock has within its UCBlock. If it is a
  *   transmission line, then its index is the index that this transmission
  *   line has within its NetworkBlock.
  *
  * - The one-dimensional variable "AssetType", of type netCDF::Uint(), which
  *   is either a scalar or indexed over "NumAssets", indicating the type of
  *   the i-th asset. For a UnitBlock, the type is 0, and for a transmission
  *   line, the type is 1. If it is a scalar, then we assume that AssetType[i]
  *   = AssetType[0] for all i in {0, ..., NumAssets - 1}. This variable is
  *   optional. If it is not provided, then we assume that AssetType[i] = 0
  *   for each i in {0, ..., NumAssets - 1}.
  *
  * - The variable "LowerBound", of type netCDF::NcDouble(), which is either a
  *   scalar or indexed over "NumAssets". If it is a scalar, then we assume
  *   that LowerBound[i] = LowerBound[0] for all i in {0, ..., NumAssets -
  *   1}. The i-th element of this vector provides the lower bound on the i-th
  *   ColVariable of this InvestmentBlock. This variable is optional. If it is
  *   not provided, then we assume that LowerBound[i] = 0 for all i in {0,
  *   ..., NumAssets - 1}.
  *
  * - The variable "InstalledQuantity", of type netCDF::NcDouble(), which is
  *   either a scalar or indexed over "NumAssets". If it is a scalar, then we
  *   assume that InstalledQuantity[i] = InstalledQuantity[0] for all i in {0,
  *   ..., NumAssets - 1}. The i-th element of this vector provides the amount
  *   of the i-th asset that is currently installed in the system and,
  *   therefore, that is not subject to investment costs. This variable is
  *   optional. If it is not provided, then we assume that
  *   InstalledQuantity[i] = 1 for all i in {0, ..., NumAssets - 1}.
  *
  * Let x[i] represent the value of the i-th active Variable of this
  * InvestmentFunction. If x[i] is greater than InstalledQuantity[i], then the
  * i-th asset is being subject to investment. If x[i] is less than
  * InstalledQuantity[i], then the i-th asset is being subject to
  * disinvestment. Because the cost of investment is tipically different from
  * the cost of disinvestment, the fixed (des)investment cost is composed by
  * two parts: the one associated with an investment and the one associated
  * with a disinvestment. The fixed cost associated with the i-th asset is
  * given by
  *
  *   c[i] * ( x[i] - InstalledQuantity[i] ) if x[i] > InstalledQuantity[i],
  *
  * and
  *
  *   d[i] * ( InstalledQuantity[i] - x[i] ) if x[i] <= InstalledQuantity[i].
  *
  * - The one-dimensional variable "Cost", of type netCDF::NcDouble(), which
  *   is either a scalar or indexed over "NumAssets", containing the fixed
  *   (linear) cost of investing in one unit of each asset. This variable thus
  *   defines the coefficients "c" (see above) of the linear term of the
  *   function represented by this InvestmentFunction. If it is a scalar, then
  *   we assume that Cost[i] = Cost[0] for all i in {0, ..., NumAssets -
  *   1}. The i-th element of this vector is associated with the i-th asset
  *   and, therefore, with the i-th active ColVariable of this
  *   InvestmentFunction. This variable is optional. If it is not provided
  *   then all investment costs are considered to be zero.
  *
  * - The one-dimensional variable "DisinvestmentCost", of type
  *   netCDF::NcDouble(), which is either a scalar or indexed over
  *   "NumAssets", containing the fixed (linear) cost of disinvesting in one
  *   unit of each asset. This variable thus defines the coefficients "d" (see
  *   above) of the linear term of the function represented by this
  *   InvestmentFunction. If it is a scalar, then we assume that
  *   DisinvestmentCost[i] = DisinvestmentCost[0] for all i in {0, ...,
  *   NumAssets - 1}. The i-th element of this vector is associated with the
  *   i-th asset and, therefore, with the i-th active ColVariable of this
  *   InvestmentFunction. This variable is optional. If it is not provided
  *   then all disinvestment costs are considered to be zero.
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

 FunctionValue get_value() const override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
 /// returns a lower estimate of the InvestmentFunction
 /** This method simply returns get_value().  */

 FunctionValue get_lower_estimate() const override {
  return( get_value() );
 }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
 /// returns an upper estimate of the InvestmentFunction
 /** This method simply returns get_value().  */

 FunctionValue get_upper_estimate() const override {
  return( get_value() );
 }

/*--------------------------------------------------------------------------*/
 /// returns the "constant term" of the InvestmentFunction

 FunctionValue get_constant_term() const override;

/*--------------------------------------------------------------------------*/
 /// returns true only if this InvestmentFunction is convex
 /** This method returns true only if this InvestmentFunction is convex. If
  * this InvestmentFunction has no sub-Block or the sense of the Objective of
  * its sub-Block is maximization, then this method returns false. Otherwise,
  * it returns true. */

 bool is_convex() const override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
 /// returns true only if this InvestmentFunction is concave
 /** This method returns true only if this InvestmentFunction is concave. If
  * this InvestmentFunction has no sub-Block or the sense of the Objective of
  * its sub-Block is minimization, then this method returns false. Otherwise,
  * it returns true. */

 bool is_concave() const override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
 /// returns true only if this InvestmentFunction is linear
 /** Method that returns true only if this InvestmentFunction is linear. In
  * particular (and probably very rare) cases, this Function could be
  * linear. We do not attempt to find this out and this method simply returns
  * \c false. */

 bool is_linear() const override { return( false ); }

/*--------------------------------------------------------------------------*/
 /// tells whether a linearization is available

 bool has_linearization( bool diagonal = true ) override final;

/*--------------------------------------------------------------------------*/
 /// store a linearization in the global pool

 void store_linearization( Index name , ModParam issueMod = eModBlck )
  override final;

/*--------------------------------------------------------------------------*/

 bool is_linearization_there( Index name ) const override final {
  return global_pool.is_linearization_there( name );
 }

/*--------------------------------------------------------------------------*/

 bool is_linearization_vertical( Index name ) const override final {
  return global_pool.is_linearization_vertical( name );
 }

/*--------------------------------------------------------------------------*/
 /// stores a combination of the given linearizations
 /** This method creates a combination of the given set of linearizations,
  * with the given coefficients, and stores it into the global pool of
  * linearizations with the given name.
  *
  * InvestmentFunction can produce two types of linearizations: diagonal and
  * vertical ones. For a combination of linearizations to be valid, it must
  * satisfy one of the following two conditions:
  *
  * -# It is a combination involving only vertical linearizations, and each
  *    coefficient (multiplier) must be nonnegative (actually, greater than or
  *    equal to - #dblAAccMlt).
  *
  * -# It is a combination involving at least one diagonal linearization, each
  *    coefficient (multiplier) must be nonnegative (actually, greater than or
  *    equal to - #dblAAccMlt), and the sum of the coefficients of the
  *    diagonal linearizations must be approximately equal to 1:
  *
  *    abs( 1 - sum coefficients of diagonal linearizations ) <= K * #dblAAccMlt
  *
  *    where K is the number of linearizations being combined.
  *
  * In the first case, the resulting linearization is a vertical one, while in
  * the second case it is a diagonal linearization. If none of the above two
  * conditions are met, an exception is thrown. */

 void store_combination_of_linearizations
 ( c_LinearCombination & coefficients , Index name ,
   ModParam issueMod = eModBlck ) override final;

/*--------------------------------------------------------------------------*/
 /// specify which linearization is "the important one"

 void set_important_linearization( LinearCombination && coefficients )
  override final {
  global_pool.set_important_linearization( std::move( coefficients ) );
 }

/*--------------------------------------------------------------------------*/
 /// return the combination used to form "the important linearization"

 c_LinearCombination & get_important_linearization_coefficients()
  const override final {
  return( global_pool.get_important_linearization_coefficients() );
 }

/*--------------------------------------------------------------------------*/
 /// delete the given linearization from the global pool of linearizations

 void delete_linearization( Index name ,
                            ModParam issueMod = eModBlck ) override final;

/*--------------------------------------------------------------------------*/

 void delete_linearizations( Subset && which , bool ordered = true ,
                             ModParam issueMod = eModBlck ) override final;

/*--------------------------------------------------------------------------*/

 void get_linearization_coefficients
 ( FunctionValue * g , Range range = std::make_pair( 0 , Inf<Index>() ) ,
   Index name = Inf<Index>() ) override;

/*--------------------------------------------------------------------------*/

 void get_linearization_coefficients
 ( SparseVector & g , Range range = std::make_pair( 0 , Inf<Index>() ) ,
   Index name = Inf<Index>() ) override;

/*--------------------------------------------------------------------------*/

 void get_linearization_coefficients
 ( FunctionValue * g , c_Subset & subset  , bool ordered = false ,
   Index name = Inf<Index>() ) override;

/*--------------------------------------------------------------------------*/

 void get_linearization_coefficients
 ( SparseVector & g , c_Subset & subset , bool ordered = false ,
   Index name = Inf<Index>() ) override;

/*--------------------------------------------------------------------------*/
 /// return the constant term of a linearization

 FunctionValue get_linearization_constant( Index name = Inf<Index>() )
  override final;

/*--------------------------------------------------------------------------*/
 /// returns a pointer to the Solver attached to the i-th sub-Block (if any)
 /** This method returns a pointer to the Solver attached to the i-th
  * sub-Block of this InvestmentFunction. The template parameter \p T
  * indicates the type of Solver whose pointer will be returned; its default
  * value is Solver. If
  *
  * - \p i is not an index corresponding to a sub-Block; or
  *
  * - the i-th sub-Block of this InvestmentFunction does not have a Solver
  *   attached to it; or
  *
  * - the Solver attached to the i-th sub-Block is not or does not derive from
  *   \p T
  *
  * then a nullptr is returned. Otherwise, a pointer of type \p T is
  * returned. */

 template<class T = Solver>
 inline T * get_solver( Index i ) const {
  if( i >= v_Block.size() )
   return nullptr;

  if( v_Block[ i ]->get_registered_solvers().empty() )
   return nullptr;

  return dynamic_cast< T * >( v_Block[ i ]->get_registered_solvers().front() );
 }

/*--------------------------------------------------------------------------*/

 /// returns true if and only if the linear constraints are satisfied
 /** This function returns true if and only if the linear constraints are
  * satisfied, considering the current values of the active Variable of this
  * InvestmentFunction.
  *
  * @return true if and only if the linear constraints are satisfied. */

 bool is_feasible();

/**@} ----------------------------------------------------------------------*/
/*-------- METHODS FOR READING THE DATA OF THE InvestmentFunction ----------*/
/*--------------------------------------------------------------------------*/
/** @name Reading the data of the InvestmentFunction
 * @{ */

 /// returns the indices of the assets that are subject to investment
 const std::vector< Index > & get_asset_indices() const {
  return v_asset_indices;
 }

/*--------------------------------------------------------------------------*/

 /// returns the types of the assets that are subject to investment
 const std::vector< AssetType > & get_asset_type() const {
  return v_asset_type;
 }

/*--------------------------------------------------------------------------*/

 /// returns the amount of the given asset currently installed in the system
 /** This function returns the amount of the given \p asset that is currently
  * installed in the system and, therefore, that is not subject to investment
  * costs.
  *
  * @param asset The index of an asset subject to investment.
  *
  * @return The amount of the given \p asset currently installed in the
  *         system. */

 double get_installed_quantity( Index asset ) const {
  if( v_installed_quantity.empty() )
   return 1;
  assert( asset < v_installed_quantity.size() );
  return v_installed_quantity[ asset ];
 }

/** @} ---------------------------------------------------------------------*/
/*-------------------- Methods for handling Modification -------------------*/
/*--------------------------------------------------------------------------*/
/** @name Methods for handling Modification
 *  @{ */

 void add_Modification( sp_Mod mod , Observer::ChnlName chnl = 0 ) override;

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

 void load( std::istream &input , char frmt = 0 ) override final;

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

 bool f_compute_linearization = true;
 ///< indicates whether a linearization must be computed

 bool f_output_solution = false;
 ///< indicates whether the solution of each UCBlock must be output

 FunctionValue AAccMlt;
 ///< maximum absolute error in the multipliers of a linear combination

 bool f_ignore_modifications = false; ///< ignore any Modification

 bool f_reformulated_bounds = false;
 ///< indicates whether the bounds on the active variables were reformulated
 /**< The active variables of this InvestmentFunction may have natural lower
  * and upper bounds in the model in which they are defined. That is, for each
  * active variable x, there may be l and u such that x must satisfy
  *
  *     l <= x <= u.
  *
  * These natural bounds, however, may have been reformulated such that x
  * becomes a nonnegative variable satisfying
  *
  *     0 <= x <= u - l
  *
  * if the lower bound l is finite. This bool variable thus indicates whether
  * the natural bounds on the active variables have been reformulated. */

 bool f_replicate_battery = false; ///< replicate battery units

 bool f_replicate_intermittent = false; ///< replicate intermittent units

 bool f_has_value = false;
 ///< the value of the Function was successfully computed

 bool f_has_diagonal_linearization = false;
 ///< a diagonal linearization is available

 Index f_num_sub_blocks = 1; ///< number of (identical) sub-Blocks

 void * f_id; ///< the "identity" of the InvestmentFunction

 double f_value;
 ///< the value of this InvestmentFunction after compute() is called

 std::vector< Index > v_block_indices_map;
 ///< map the index of an UnitBlock to the index of the asset under investment

 std::vector< Index > v_asset_indices;
 ///< indices of the assets that are subject to investment

 std::vector< AssetType > v_asset_type;
 ///< the type of each asset that is subject to investment

 std::vector< double > v_linearization;
 ///< linearization associated with the most recent call to compute()

 std::vector< double > v_cost;
 ///< the cost of investing in one unit of each asset

 std::vector< double > v_disinvestment_cost;
 ///< the cost of disinvesting in one unit of each asset

 std::vector< double > v_installed_quantity;
 ///< amount of each asset currently installed in the system

 std::vector< std::vector< std::vector< Index > > > generator_node_map;
 ///< maps the index of a generator to the node it belongs to
 /**< This vector maps a generator to the node it belongs to. The generator is
  * identified by a triplet (stage, unit_block_index, generator_index), where
  * stage indicates the stage (between 0 and SDDPBlock::get_time_horizon() -
  * 1), unit_block_index identifies the UnitBlock to which the generator
  * belongs (and index between 0 and v_block_indices.size() - 1) and
  * generator_index is the index of the generator within its UnitBlock. */

 std::vector< double > v_lower_bound;
 ///< lower bound on the value of the active variables

 MultiVector v_A;
 ///< the coefficient matrix of the linear constraints

 RealVector v_constraints_lower_bound;
 ///< the lower bound of the linear constraints

 RealVector v_constraints_upper_bound;
 ///< the upper bound of the linear constraints

 ViolatedConstraint f_violated_constraint;
 ///< it indicates which linear constraint has been violated

/*--------------------------------------------------------------------------*/
/*--------------------- PRIVATE PART OF THE CLASS --------------------------*/
/*--------------------------------------------------------------------------*/

 private:

/*--------------------------------------------------------------------------*/
/*-------------------------- PRIVATE CLASSES -------------------------------*/
/*--------------------------------------------------------------------------*/

 /// A convenience class for representing the global pool of linearizations
 class GlobalPool {

 public:

  static constexpr auto NaN = std::numeric_limits<FunctionValue>::quiet_NaN();

/*--------------------------------------------------------------------------*/

  GlobalPool() = default;

/*--------------------------------------------------------------------------*/

  virtual ~GlobalPool() {}

/*--------------------------------------------------------------------------*/
  // resizes the global pool
  /** Resize the global pool to have the given \p size. It is important to
   * notice that
   *
   *         IF THE SIZE OF THE POOL IS BEING DECREASED, ANY LINEARIZATION
   *         WHOSE name IS GREATER THAN OR EQUAL TO THE GIVEN NEW size IS
   *         DESTROYED.
   *
   * @param size The size of the global pool.
   */

  void resize( Index size );

/*--------------------------------------------------------------------------*/
  /// returns the size of the global pool

  Index size() const { return( linearization_constants.size() ); }

/*--------------------------------------------------------------------------*/
  /// stores the given linearization constant and solution in the global pool
  /** This function stores the given linearization constant and solution into
   * the global pool under the given \p name. If the given \p name is invalid,
   * an exception is thrown. If a Solution is currently stored under the given
   * \p name, this Solution is destroyed.
   *
   * @param constant the value of the linearization constant.
   *
   * @param coefficients the coefficients of the linearization.
   *
   * @param name the name under which the linearization will be stored.
   *
   * @param diagonal indicates whether the linearization is a diagonal one. */

  void store( FunctionValue constant ,
              std::vector< FunctionValue > coefficients ,
              Index name , bool diagonal );

/*--------------------------------------------------------------------------*/
  /// tells if there is a linearization in this GlobalPool with the given name
  /** This method returns true if \p name is the index (name) of a
   * linearization currently in this GlobalPool. */

  bool is_linearization_there( Index name ) const;

/*--------------------------------------------------------------------------*/
 /// tells if the linearization in this GlobalPool with that name is vertical
 /** This method returns true if \p name is the index (name) of a vertical
  * linearization currently in this GlobalPool. */

  bool is_linearization_vertical( Index name ) const;

/*--------------------------------------------------------------------------*/
  /// returns the linearization constant stored under the given name
  /** This function returns the value of the linearization constant that is
   * stored under the given \p name. If the given \p name is invalid, an
   * exception is thrown.
   *
   * @param name the name of the desired constant.
   *
   * @return the value of the linearization constant that is stored under the
   *         given \p name. */

  FunctionValue get_linearization_constant( Index name ) const {
   if( name < size() )
    return linearization_constants[ name ];
   throw( std::invalid_argument
          ( "InvestmentFunction::GlobalPool::get_linearization_constant: "
            "linearization with name " + std::to_string( name ) +
            " does not exist." ) );
  }

/*--------------------------------------------------------------------------*/
  /// sets the linearization constant under the given name
  /** This function sets the value of the linearization constant under the
   * given \p name. If the given \p name is invalid, an exception is thrown.
   *
   * @param constant the value of the linearization constant to be stored.
   *
   * @param name the name under which the constant will be stored. */

  void set_linearization_constant( FunctionValue constant , Index name ) {
   if( name >= size() )
    throw( std::invalid_argument
           ( "InvestmentFunction::GlobalPool::set_linearization_constant: "
             "linearization with name " + std::to_string( name ) +
             " does not exist." ) );

   linearization_constants[ name ] = constant;
  }

/*--------------------------------------------------------------------------*/
  /// invalidates all linearizations
  /** This function invalidates all linearizations, by setting NaN to each
   * linearization constant currently stored. This means that any
   * linearization previously computed may no longer be valid. The
   * linearizations, however, remain stored in this global pool. If they
   * should be destroyed, explicit calls to delete_linearization() must be
   * made. */

  void invalidate() {
   linearization_constants.assign( linearization_constants.size() , NaN );
  }

/*--------------------------------------------------------------------------*/

  void set_important_linearization( LinearCombination && coefficients ) {
   important_linearization_lin_comb = std::move( coefficients );
  }

/*--------------------------------------------------------------------------*/
  /// return the combination used to form "the important linearization"

  c_LinearCombination & get_important_linearization_coefficients() const {
   return important_linearization_lin_comb;
  }

/*--------------------------------------------------------------------------*/
  /// stores a combination of the linearizations that are already stored
  /** This method creates a linear combination of a given set of
   * linearizations (specified by \p linear_combination) and stores it into
   * the global pool of linearizations with the given \p name (which must be
   * an integer between 0 and size() - 1). If \p linear_combination is empty,
   * an exception is thrown. If any of the names in the given \p
   * linear_combination is invalid, an exception is thrown. If the given \p
   * name is invalid, an exception is thrown.
   *
   * @param linear_combination the LinearCombination containing the names of
   *        the linearizations and their respective coefficients in the
   *        combination.
   *
   * @param name the name under which the combination of linearizations will
   *        be stored.
   *
   * @param AAccMlt the maximum absolute error in the multipliers. */

  void store_combination_of_linearizations
  ( c_LinearCombination & linear_combination , Index name ,
    FunctionValue AAccMlt );

/*--------------------------------------------------------------------------*/
  /// deletes the linearization with the given name
  /** This function deletes the linearization with the given \p name. If the
   * given \p name is invalid, an exception is thrown.
   *
   * @param name the name of the linearization to be deleted. */

  void delete_linearization( Index name );

/*--------------------------------------------------------------------------*/
  /// deletes the linearizations with the given names
  /** This function deletes the linearizations with the given names given in
   * \p which. If any given name is invalid, an exception is thrown.
   *
   * @param which the names of the linearizations that must be deleted.
   */
  void delete_linearizations( Subset & which , bool ordered );

/*--------------------------------------------------------------------------*/

 private:

  std::vector< FunctionValue > linearization_constants;
  ///< linearization constants

  std::vector< std::vector< FunctionValue > > linearization_coefficients;
  ///< linearization coefficients

  LinearCombination important_linearization_lin_comb;
  ///< the linear combination of the important linearization

  std::vector< bool > is_diagonal;
  ///< indicates whether a linearization is diagonal

 }; // end( class( GlobalPool ) )

/*--------------------------------------------------------------------------*/
/*-------------------------- PRIVATE METHODS -------------------------------*/
/*--------------------------------------------------------------------------*/

 /// Update the given UnitBlock according to the given \p investment
 /** This function updates the given UnitBlock according to the given \p
  * investment.
  *
  * @param block A pointer to a UnitBlock.
  *
  * @param investment The investment to be made in the given UnitBlock. */

 void update_unit_block( UnitBlock * block , double investment );

/*--------------------------------------------------------------------------*/

 /// Update a set of UnitBlock according to the given \p investment
 /** This function updates a set of UnitBlock, given by their indices \p
  * block_indices, according to the given \p investment.
  *
  * @param sub_block_index The index of a sub-Block of this
  *        InvestmentFunction.
  *
  * @param block_indices Indices of the UnitBlock which must be updated.
  *
  * @param investment The investment to be made in each UnitBlock. */

 void update_unit_blocks( Index sub_block_index ,
                          const std::vector< Index > & block_indices ,
                          const std::vector< double > & investment );

/*--------------------------------------------------------------------------*/

 /// Update a set of line according to the given \p investment
 /** This function updates a set of transmission lines, given by their indices
  * \p line_indices, according to the given \p investment.
  *
  * @param sub_block_index The index of a sub-Block of this
  *        InvestmentFunction.
  *
  * @param line_indices Indices of the transmission lines which must be
  *        updated.
  *
  * @param investment The investment to be made in each line. */

 void update_network_blocks( Index sub_block_index ,
                             const std::vector< Index > & line_indices ,
                             const std::vector< double > & investment );

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
  * with the given \p stage in the \p i-th sub-Block..
  *
  * @param stage A stage.
  *
  * @param i The index of a sub-Block.
  *
  * @return A pointer to the Solver of the UCBlock associated with the given
  *         \p stage in the \p i-th sub-Block. */

 CDASolver * get_ucblock_solver( Index stage , Index i ) const;

/*--------------------------------------------------------------------------*/

 /// returns a pointer to the UCBlock associated with the given \p stage
 /** This function returns a pointer to the UCBlock associated with
  * the given \p stage in the \p i-th sub-Block.
  *
  * @param stage A stage.
  *
  * @param i The index of a sub-Block.
  *
  * @return A pointer to the UCBlock associated with the given \p stage in the
  *         \p i-th sub-Block. */

 UCBlock * get_ucblock( Index stage , Index i ) const;

/*--------------------------------------------------------------------------*/

 /// returns a pointer to the i-th SDDPBlock
 /** This function returns a pointer to the i-th SDDPBlock.
  *
  * @param i The index of a sub-Block of this InvestmentFunction.
  *
  * @return A pointer to the i-th SDDPBlock of this InvestmentFunction. */

 SDDPBlock * get_sddp_block( Index i ) const;

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

 double compute_scale_linearization( Index block_index , Index stage ,
                                     Index sub_block_index );

/*--------------------------------------------------------------------------*/

 /// returns the contribution to the linearization by the given Block
 /** This function computes and returns the contribution to the linearization
  * by the given IntermittentUnitBlock, considering the constraints that are
  * affected by kappa.
  *
  * @param unit A pointer to the IntermittentUnitBlock.
  *
  * @param var_index The index of the active Variable associated with the
  *        IntermittentUnitBlock's kappa.
  *
  * @return the contribution to the linearization by the given
  *         IntermittentUnitBlock, considering the constraints that are
  *         affected by kappa. */

 double compute_kappa_linearization( IntermittentUnitBlock * unit ,
                                     Index var_index );

/*--------------------------------------------------------------------------*/

 /// returns the contribution to the linearization by the given Block
 /** This function computes and returns the contribution to the linearization
  * by the given BatteryUnitBlock, considering the constraints that are
  * affected by kappa.
  *
  * @param unit A pointer to the BatteryUnitBlock.
  *
  * @param var_index The index of the active Variable associated with the
  *        BatteryUnitBlock's kappa.
  *
  * @return the contribution to the linearization by the given
  *         BatteryUnitBlock, considering the constraints that are affected by
  *         kappa. */

 double compute_kappa_linearization( const BatteryUnitBlock * unit ,
                                     Index var_index );

/*--------------------------------------------------------------------------*/

 /// updates the linearization to reflect the most recent scenario considered
 /** This function updates the linearization to reflect the most recent
  * scenario considered, whose subproblem was solved by the Solver attached to
  * the sub-Block whose index is \p sub_block_index.
  *
  * @param sub_block_index The index of the sub-Block which will be used to
  *        update the linearization. */

 void update_linearization( Index sub_block_index );

/*--------------------------------------------------------------------------*/

 /// updates the linearization with respect to the set of UnitBlock
 void update_linearization_unit_blocks
 ( Index stage , Index sub_block_index ,
   const std::vector< std::pair< Index , Index > > & block_indices );

/*--------------------------------------------------------------------------*/

 /// updates the linearization with respect to the set of NetworkBlock
 void update_linearization_network_blocks
 ( Index stage , Index sub_block_index ,
   const std::vector< std::pair< Index , Index > > & line_indices );

/*--------------------------------------------------------------------------*/

 /// returns the node to which the given generator belongs
 Index get_node( Index stage , Index unit_block_index , Index generator ) const;

/*--------------------------------------------------------------------------*/

 /// builds the mapping between generator and the node it belongs to
 void build_generator_node_map();

/*--------------------------------------------------------------------------*/

 /// returns a pointer to the BendersBFunction associated with the given stage
 /** This function returns a pointer to the BendersBFunction associated with
  * the given \p stage in the \p i-th sub-Block.
  *
  * @param stage A stage.
  *
  * @param i The index of a sub-Block.
  *
  * @return A pointer to the BendersBFunction associated with the given \p
  *         stage in the \p i-th sub-Block. */

 BendersBFunction * get_benders_function( Index stage , Index i ) const;

/*--------------------------------------------------------------------------*/

 /// returns the value of the i-th active variable
 /** This function returns the value of the i-th active variable, possibly
  * taking into account its lower bound. If \p atual is \c true, then this
  * function returs the variable of the i-th active variable. Otherwise, if
  * the lower bound for this variable is finite, then this function returns
  * the value of this active variable plus its lower bound.
  *
  * This function is useful because the model to which this variable belongs
  * may have been reformulated as follows. This active variable, let us call
  * it x, may be subject to lower and upper bounds, so that it must satisfy
  *
  *     l <= x <= u
  *
  * in the model in which it is defined. However, this model may have been
  * reformulated in such a way that, if the lower bound l is finite, this
  * variable becomes a nonnegative variable which must then satisfy
  *
  *     0 <= x <= u - l.
  *
  * Thus, if the model have been reformulated in this way, l is finite
  * (actually, not minus infinity), and \p actual is false, then this function
  * returns x + l. Otherwise, it returns the value of x. */

 double get_var_value( Index i , bool actual = true ) const {
  if( f_reformulated_bounds && ( ! actual ) && ( i < v_lower_bound.size() ) &&
      ( v_lower_bound[ i ] > -Inf< double >() ) )
   return v_x[ i ]->get_value() + v_lower_bound[ i ];
  return v_x[ i ]->get_value();
 }

/*--------------------------------------------------------------------------*/

 /// returns the lower bound for the given active variable
 /** This function returns the lower bound for the value of the i-th active
  * variable of this InvestmentFunction.
  *
  * @para i The index of an active variable of this InvestmentFunction.
  *
  * @return the lower bound for the value of the i-th active variable of this
  *         InvestmentFunction. */

 double get_var_lower_bound( Index i ) const {
  if( i < v_lower_bound.size() )
   return v_lower_bound[ i ];
  return -Inf< double >();
 }

/*--------------------------------------------------------------------------*/

 /// returns the investment cost of the i-th asset
 double get_cost( Index i ) const {
  if( i < v_cost.size() )
   return v_cost[ i ];
  return 0;
 }

/*--------------------------------------------------------------------------*/

 /// returns the disinvestment cost of the i-th asset
 double get_disinvestment_cost( Index i ) const {
  if( i < v_disinvestment_cost.size() )
   return v_disinvestment_cost[ i ];
  return 0;
 }

/*--------------------------------------------------------------------------*/

 /// returns the number of scenarios
 Index get_number_scenarios() const;

/*--------------------------------------------------------------------------*/

 /// returns the number of stages
 Index get_number_stages() const;

/*--------------------------------------------------------------------------*/

 /// locks a(n unlocked) sub-Block and returns its index
 Index lock_sub_block();

/*--------------------------------------------------------------------------*/

 /// unlocks the i-th sub-Block
 void unlock_sub_block( Index i );

/*--------------------------------------------------------------------------*/

 SMSpp_insert_in_factory_h; // insert InvestmentFunction in the Block factory

/*--------------------------------------------------------------------------*/
/*---------------------------- PRIVATE FIELDS  -----------------------------*/
/*--------------------------------------------------------------------------*/

 /// It indicates whether each sub-Block is locked
 std::vector< bool > is_locked;

 /// This is the waiting time before trying to acquire the lock again
 double waiting_time = 1e-4;

 /// Global pool of linearizations
 GlobalPool global_pool;

 /// Name of the netCDF sub-group containing the description of the inner Block
 inline static const std::string BLOCK_NAME = "SDDPBlock";

/*--------------------------------------------------------------------------*/
/*---------------------------- PRIVATE METHODS  ----------------------------*/
/*--------------------------------------------------------------------------*/

 double compute_linear_constraint_value( Index i ) const;

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
