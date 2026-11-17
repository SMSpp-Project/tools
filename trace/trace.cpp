/*--------------------------------------------------------------------------*/
/*--------------------------- File trace.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * A small tool that solves one instance with every Solver of a
 * BlockSolverConfig, in turn and from the same Block, and writes for each a
 * trace of its search as JSON lines, in the format of the arena of
 * opt-playground (https://github.com/dmeoli/opt-playground, section 5.1 of
 * docs/study-search-games.md): a header, one line per event of the search,
 * and the result. The events are those that the Solver gives to its
 * handlers: the steps of the SAT solver of a SATSolver (event eSATSearch,
 * with intTrace set in its ComputeConfig), the nodes of a BranchAndXSolver
 * (eEveryNode), the incumbent and the bound of a :MILPSolver (eProgress); a
 * Solver with none of these gives the result only.
 *
 * The instance is a SATBlock out of a DIMACS file (.cnf, or .wcnf for the
 * weighted partial MaxSAT), whose Solver may be those of its physical
 * representation as well as those of its abstract one, or an AbstractBlock
 * out of an MPS (.mps) or LP (.lp) file.
 *
 * Usage: trace <instance> <BlockSolverConfig> <output prefix> [<header>
 *              [<cap>]]
 *
 * Solver k writes <output prefix>-<k>.jsonl. The members of the JSON object
 * in the file <header> (e.g., "game", "instance", "encoding", "machine")
 * are copied into the header of each trace, after those the tool writes
 * ("kind", "v", "sample", "solver", "counters"). At most <cap> events
 * (1000000 by default) are written for each Solver; the counters of the
 * result count them all.
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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "AbstractBlock.h"
#include "BlockSolverConfig.h"
#include "BranchAndXSolver.h"
#include "MILPSolver.h"
#include "SATBlock.h"
#include "SATSolver.h"

#include "common_utils.h"

#if __has_include( <SMS++_toolsConfig.h> )
 #include <SMS++_toolsConfig.h>
#endif

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

namespace
{

// a number of JSON: null if not finite
std::string num( double v )
{
 if( ! std::isfinite( v ) )
  return( "null" );
 std::ostringstream o;
 o << std::setprecision( 12 ) << v;
 return( o.str() );
 }

// a list of integers of JSON
std::string list( const std::vector< int > & v )
{
 std::ostringstream o;
 o << "[";
 for( std::size_t i = 0 ; i < v.size() ; ++i )
  o << ( i ? "," : "" ) << v[ i ];
 o << "]";
 return( o.str() );
 }

// a string of JSON
std::string str( const std::string & s )
{
 std::string o = "\"";
 for( char c : s )
  if( ( c == '"' ) || ( c == '\\' ) )
   o += std::string( "\\" ) + c;
  else
   o += ( c == '\n' ) || ( c == '\t' ) ? ' ' : c;
 return( o + "\"" );
 }

// the members of the JSON object in a file, without its braces
std::string members( const std::string & file )
{
 std::ifstream in( file );
 if( ! in )
  throw( std::invalid_argument( "cannot open " + file ) );
 std::string s( ( std::istreambuf_iterator< char >( in ) ) ,
		std::istreambuf_iterator< char >() );
 const auto b = s.find( '{' );
 const auto e = s.rfind( '}' );
 if( ( b == std::string::npos ) || ( e == std::string::npos ) || ( e < b ) )
  throw( std::invalid_argument( file + " is not a JSON object" ) );
 s = s.substr( b + 1 , e - b - 1 );
 const auto f = s.find_first_not_of( " \t\r\n" );
 return( f == std::string::npos ? "" : s.substr( f ) );
 }

// the fixings that a Change made by the branching of a SATSolver adds to
// those of the father (literals of DIMACS), false if it is of another kind
bool fixings( const Change * chg , std::vector< int > & fix )
{
 if( auto g = dynamic_cast< const GroupChange * >( chg ) ) {
  for( auto s : g->sub_Changes() )
   if( ! fixings( s , fix ) )
    return( false );
  return( true );
  }
 auto s = dynamic_cast< const SATBlockChange * >( chg );
 if( ! s )
  return( false );
 const auto & nms = s->nms();
 for( std::size_t i = 0 ; i < nms.size() ; ++i ) {
  const int v = int( nms[ i ] ) + 1;
  std::erase_if( fix , [ v ]( int l ) { return( std::abs( l ) == v ); } );
  if( s->type() == SATBlockChange::eFixX )
   fix.push_back( s->values()[ i ] > 0.5 ? v : - v );
  }
 return( true );
 }

void usage( std::ostream & os , const char * exe )
{
 os << "Usage: " << exe << " <instance> <BlockSolverConfig> <output prefix>"
    << " [<header> [<cap>]]" << std::endl
    << "  or:  " << exe << " -h | --help" << std::endl
    << "  or:  " << exe << " -V | --version" << std::endl;
 }

void docopt( const char * exe )
{
 usage( std::cout , exe );
 std::cout
   << std::endl
   << "Solves the <instance> by every Solver of the <BlockSolverConfig> in\n"
      "turn and writes for each the trace of its search, as JSON lines, in\n"
      "<output prefix>-<k>.jsonl: the steps of the SAT solver of a\n"
      "SATSolver (with intTrace 1), the nodes of a BranchAndXSolver, the\n"
      "incumbent and the bound of a MILPSolver, then the result. The\n"
      "members of the JSON object in <header> are copied into the header of\n"
      "each trace; at most <cap> events are written for each Solver\n"
      "(1000000 by default).\n"
      "\n"
      "The <instance> is a DIMACS .cnf or .wcnf file, read into a SATBlock,\n"
      "or an .mps or .lp file, read into an AbstractBlock.\n"
      "\n"
      "Options:\n"
      "  -h, --help     print this help\n"
      "  -V, --version  print the SMS++ tools version and exit\n"
      "\n"
      "Examples:\n"
      "  smspp_trace australia.cnf ColBSCfg.txt australia australia.json\n"
      "      writes australia-1.jsonl, ... (run in trace/config)\n"
      "\n"
      "Exit status:\n"
      "  0 if all the traces are written, nonzero otherwise."
   << std::endl;
 }

/*--------------------------------------------------------------------------*/
// the trace of one Solver: the header, the events and the result

class Trace
{
 public:

 Trace( const std::string & file , long cap ,
	std::vector< std::string > counters )
  : o( file ) , f_cap( cap ) , v_names( std::move( counters ) ) ,
    v_n( v_names.size() , 0 ) {
  if( ! o )
   throw( std::invalid_argument( "cannot write " + file ) );
  }

 std::ofstream o;

 // the header: what the tool writes, then the members of the file
 void header( const std::string & solver , const std::string & extra ) {
  o << "{\"kind\":\"arena\",\"v\":1,\"sample\":false,\"solver\":" << solver
    << ",\"counters\":[";
  for( std::size_t i = 0 ; i < v_names.size() ; ++i )
   o << ( i ? "," : "" ) << str( v_names[ i ] );
  o << "]" << ( extra.empty() ? "" : "," ) << extra << "}\n";
  }

 void start( void ) { f_start = std::chrono::steady_clock::now(); }

 // the counter i, which the following events carry
 long & n( std::size_t i ) { return( v_n[ i ] ); }

 // an event with its fields, which start with a comma
 void event( const char * type , const std::string & fields ) {
  if( f_events++ >= f_cap )
   return;
  o << "{\"step\":" << f_events - 1 << ",\"type\":\"" << type << "\",\"t\":"
    << num( std::chrono::duration< double >(
	     std::chrono::steady_clock::now() - f_start ).count() )
    << ",\"n\":{";
  for( std::size_t i = 0 ; i < v_names.size() ; ++i )
   o << ( i ? "," : "" ) << str( v_names[ i ] ) << ":" << v_n[ i ];
  o << "}" << fields << "}\n";
  }

 // a propagation: those at the same level go in one event, written when
 // something else comes
 void propagate( int lit , int level ) {
  if( ( ! v_lits.empty() ) && ( level != f_lits_level ) )
   flush();
  f_lits_level = level;
  v_lits.push_back( lit );
  }

 void flush( void ) {
  if( v_lits.empty() )
   return;
  event( "propagate" , ",\"level\":" + std::to_string( f_lits_level ) +
	 ",\"lits\":" + list( v_lits ) );
  v_lits.clear();
  }

 [[nodiscard]] long events( void ) const { return( f_events ); }

 [[nodiscard]] bool capped( void ) const { return( f_events > f_cap ); }

 private:

 long f_cap;
 long f_events = 0;
 std::vector< std::string > v_names;
 std::vector< long > v_n;
 std::vector< int > v_lits;
 int f_lits_level = 0;
 std::chrono::steady_clock::time_point f_start;
 };

}  // end of the unnamed namespace

/*--------------------------------------------------------------------------*/
/*-------------------------------- main() ----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 if( argc == 2 ) {
  const std::string opt( argv[ 1 ] );
  if( ( opt == "-h" ) || ( opt == "--help" ) ) {
   docopt( argv[ 0 ] );
   return( 0 );
   }
  if( ( opt == "-V" ) || ( opt == "--version" ) ) {
   #ifdef SMSpp_tools_VERSION
   std::cout << "SMS++ tools version " << SMSpp_tools_VERSION << std::endl;
   #else
   std::cout << "SMS++ tools version not available" << std::endl;
   #endif
   return( 0 );
   }
  }

 if( ( argc < 4 ) || ( argc > 6 ) ) {
  usage( std::cerr , argv[ 0 ] );
  return( 1 );
  }

 try {
  const std::string file( argv[ 1 ] );
  const std::string extra = ( argc >= 5 ) ? members( argv[ 4 ] ) : "";
  const long cap = ( argc == 6 ) ? std::stol( argv[ 5 ] ) : 1000000;
  const auto dot = file.rfind( '.' );
  const std::string ext = dot == std::string::npos ? ""
						   : file.substr( dot + 1 );
  std::ifstream in( file );
  if( ! in )
   throw( std::invalid_argument( "cannot open " + file ) );

  Block * block;
  SATBlock * sat = nullptr;
  if( ( ext == "cnf" ) || ( ext == "wcnf" ) ) {
   sat = new SATBlock();
   sat->load( in , ext == "wcnf" ? 'W' : 'D' );
   // the Solver of the abstract representation need it, the others not
   sat->generate_abstract_variables();
   sat->generate_abstract_constraints();
   sat->generate_objective();
   block = sat;
   }
  else
   if( ( ext == "mps" ) || ( ext == "lp" ) ) {
    auto ab = new AbstractBlock();
    ab->load( in , ext == "mps" ? 'M' : 'L' );
    block = ab;
    }
   else
    throw( std::invalid_argument( file + ": not a .cnf, .wcnf, .mps or .lp"
				  " file" ) );
  // a CNF asks for a solution, the others for an optimal one
  const bool decision = ( ext == "cnf" );
  const bool minimizing =
   block->get_objective_sense() == Objective::eMin;

  auto bsc = get_blocksolverconfig( argv[ 2 ] );
  bsc->apply( block );

  std::string code = "smspp tools";
  #ifdef SMSpp_tools_VERSION
  code += std::string( " " ) + SMSpp_tools_VERSION;
  #endif

  std::map< std::string , int > ids;  // the ids used so far
  int k = 0;
  for( auto * slv : block->get_registered_solvers() ) {
   ++k;
   const auto satslv = dynamic_cast< SATSolver * >( slv );
   const auto bnx = dynamic_cast< BranchAndXSolver * >( slv );
   const auto milp = dynamic_cast< MILPSolver * >( slv );

   // the id: the SAT solver, the back-end of the MILP, bnx; made unique
   std::string id = slv->classname();
   if( satslv )
    id = satslv->signature().substr( 0 , satslv->signature().find( '-' ) );
   else
    if( bnx )
     id = "bnx";
    else
     if( const auto p = id.find( "MILPSolver" ) ; p != std::string::npos )
      id = id.substr( 0 , p );
   std::transform( id.begin() , id.end() , id.begin() ,
		   []( unsigned char c ) { return( std::tolower( c ) ); } );
   if( const int seen = ids[ id ]++ )
    id += "-" + std::to_string( seen + 1 );

   const char * paradigm = satslv ? "sat" : bnx ? "bb" : milp ? "milp" : "";
   std::ostringstream solver;
   solver << "{\"id\":" << str( id ) << ",\"name\":"
	  << str( slv->classname() ) << ",\"block\":"
	  << str( block->classname() ) << ",\"paradigm\":"
	  << str( paradigm ) << ",\"code\":" << str( code );
   if( satslv )
    solver << ",\"backend\":" << str( satslv->signature() );
   solver << ",\"config\":" << str( get_filename( argv[ 2 ] ) + "#" +
				    std::to_string( k ) ) << "}";

   std::vector< std::string > names;
   if( satslv )
    names = { "decisions" , "propagations" , "conflicts" };
   else
    if( bnx || milp )
     names = { "nodes" };
   const std::string out =
     std::string( argv[ 3 ] ) + "-" + std::to_string( k ) + ".jsonl";
   Trace T( out , cap , names );
   T.header( solver.str() , extra );

   Solver::EventID eid = 0;
   int type = -1;
   std::map< int , std::vector< int > > fix;  // fixings of each node
   double incumbent = minimizing ? Inf< double >() : - Inf< double >();
   if( satslv ) {
    type = SATSolver::eSATSearch;
    eid = slv->set_event_handler( type , [ & ]( void ) {
     using SS = SATSolver::SearchStep;
     const auto & s = satslv->get_search_step();
     const int lit = ( s.var < 0 ) ? 0 : ( s.value ? s.var + 1
						   : - ( s.var + 1 ) );
     if( s.type == SS::ePropagate ) {
      ++T.n( 1 );
      if( lit )  // a variable of OLL is not one of the instance
       T.propagate( lit , s.level );
      return( ThinComputeInterface::eContinue );
      }
     T.flush();
     const std::string level = ",\"level\":" + std::to_string( s.level );
     switch( s.type ) {
      case( SS::eDecide ):
       ++T.n( 0 );
       if( lit )
	T.event( "decide" , level + ",\"lit\":" + std::to_string( lit ) );
       break;
      case( SS::eConflict ):
       ++T.n( 2 );
       T.event( "conflict" , level );
       break;
      case( SS::eBacktrack ):
       T.event( "backtrack" , level );
       break;
      case( SS::eRestart ):
       T.event( "backtrack" , level + ",\"restart\":true" );
       break;
      default:  // a model: the result says it
       break;
      }
     return( ThinComputeInterface::eContinue );
     } );
    }
   else
    if( bnx ) {
     type = BranchAndXSolver::eEveryNode;
     eid = slv->set_event_handler( type , [ & ]( void ) {
      using NS = BranchAndXSolver::NodeStep;
      static const char * status[] = { "open" , "branched" , "pruned" ,
				       "infeasible" , "feasible" , "pruned" };
      const auto & nd = bnx->get_node_step();
      std::string fields = ",\"id\":" + std::to_string( nd.id ) +
       ",\"parent\":" + ( nd.parent < 0 ? std::string( "null" )
			  : std::to_string( nd.parent ) ) +
       ",\"depth\":" + std::to_string( nd.depth ) + ",\"status\":\"" +
       status[ nd.status ] + "\",\"bound\":" + num( nd.bound );
      if( nd.status == NS::eUnresolved )
       fields += ",\"unresolved\":true";
      if( fix.find( nd.id ) == fix.end() ) {  // a new node
       ++T.n( 0 );
       std::vector< int > f;
       if( nd.parent >= 0 )
	f = fix[ nd.parent ];
       if( nd.change && ( ! fixings( nd.change , f ) ) ) {
	std::ostringstream p;
	p << *nd.change;
	fields += ",\"change\":" + str( p.str() );
	}
       fix[ nd.id ] = std::move( f );
       }
      fields += ",\"fix\":" + list( fix[ nd.id ] );
      T.event( "node" , fields );
      if( nd.incumbent != incumbent ) {
       incumbent = nd.incumbent;
       T.event( "bound" , minimizing
		? ",\"lb\":null,\"ub\":" + num( incumbent )
		: ",\"lb\":" + num( incumbent ) + ",\"ub\":null" );
       }
      return( ThinComputeInterface::eContinue );
      } );
     }
    else
     if( milp ) {
      type = MILPSolver::eProgress;
      eid = slv->set_event_handler( type , [ & ]( void ) {
       const auto & g = milp->get_progress();
       T.n( 0 ) = g.nodes;
       const double lb = minimizing ? g.bound : g.incumbent;
       const double ub = minimizing ? g.incumbent : g.bound;
       T.event( "bound" , ",\"lb\":" + num( lb ) + ",\"ub\":" + num( ub ) );
       return( ThinComputeInterface::eContinue );
       } );
      }

   T.start();
   const auto start = std::chrono::steady_clock::now();
   const int st = slv->compute( false );
   const double seconds = std::chrono::duration< double >(
			   std::chrono::steady_clock::now() - start ).count();
   if( type >= 0 )
    slv->reset_event_handler( type , eid );
   T.flush();

   const bool solved = slv->has_var_solution();
   std::string status;
   switch( st ) {
    case( Solver::kOK ):
     status = decision ? "sat" : "optimal";
     break;
    case( Solver::kInfeasible ):
     status = decision ? "unsat" : "infeasible";
     break;
    default:
     status = solved ? ( decision ? "sat" : "feasible" ) : "limit";
    }

   T.o << "{\"type\":\"result\",\"status\":\"" << status
       << "\",\"seconds\":" << num( seconds ) << ",\"lb\":"
       << num( slv->get_lb() ) << ",\"ub\":" << num( slv->get_ub() );
   if( sat && solved ) {  // the true literals of the solution
    slv->get_var_solution();
    std::vector< int > model;
    const auto & x = sat->get_variables();
    for( std::size_t i = 0 ; i < x.size() ; ++i )
     if( x[ i ].get_value() > 0.5 )
      model.push_back( int( i ) + 1 );
    T.o << ",\"cost\":" << num( sat->get_objective_value() )
	<< ",\"feasible\":" << ( sat->is_feasible() ? "true" : "false" )
	<< ",\"model\":" << list( model );
    }
   T.o << ",\"stats\":{";
   if( satslv ) {  // those of the SAT solver, -1 if it does not count
    const auto s = satslv->get_search_statistics();
    T.o << "\"decisions\":" << s.decisions << ",\"propagations\":"
	<< s.propagations << ",\"conflicts\":" << s.conflicts;
    }
   else
    if( bnx )
     T.o << "\"nodes\":" << T.n( 0 ) << ",\"evaluated\":"
	 << bnx->get_evaluated_nodes();
    else
     if( milp )
      T.o << "\"nodes\":" << T.n( 0 ) << ",\"iterations\":"
	  << milp->get_elapsed_iterations();
   T.o << "},\"events\":" << T.events() << ",\"capped\":"
       << ( T.capped() ? "true" : "false" ) << "}\n";

   std::cout << slv->classname() << " #" << k << " (" << id << "): "
	     << status << ", lb " << num( slv->get_lb() ) << ", ub "
	     << num( slv->get_ub() ) << ", " << T.events() << " events, "
	     << num( seconds ) << " s -> " << out << std::endl;
   }

  bsc->clear();
  bsc->apply( block );
  delete bsc;
  delete block;
  }
 catch( std::exception & e ) {
  std::cerr << e.what() << std::endl;
  return( 1 );
  }

 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*------------------------- End File trace.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
