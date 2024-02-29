/*--------------------------------------------------------------------------*/
/*------------------------- File chgcfg.cpp --------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * A simple tool to read a configuration text file and a set of pairs
 * < parameter name, value > from the command line and produce a modified
 * version of the configuration text file whereby each parameter in the list
 * is given the provided value instead of the original one.
 *
 * The assumptions are:
 *
 * - '#' is the comment character, and everything from it to the end of the
 *   line is comment
 *
 * - each pair < parameter , value > in the file is at the beginning of a
 *   separate line (possibly with trailing whitespaces)
 *
 * Note that EACH PARAMETER IN THE COMMAND LINE IS ONLY REPLACED ONCE.
 * That is, the first time (scanning the file from the beginning to the end)
 * that the parameter is found it is put in the output file with the replaced
 * value, but from then on that parameter is ignored. This allows to replace
 * parameters that occur multiple times in the file by specifying them
 * multiple times in the command line: the first command-line copy modifies
 * the first occurrence in the file and so on.
 *
 * With the BAREBONES option (see MACROS), the produced configuration file is
 * stripped of any non-necessary information (comments and whitespaces),
 * although empty and all-whitespace lines are kept.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*-------------------------------- MACROS ----------------------------------*/
/*--------------------------------------------------------------------------*/

#define BAREBONES 1
// if BAREBONES != 0, it strips every non-necessary comment and whitespace
// from the produced cfg file to produce a "barebones" (modified) version

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // check command line parameters
 if( ( argc < 3 ) || ( ! ( argc % 2 ) ) ) {
  std::cerr << "Usage: " << argv[ 0 ]
       << "in_cfg out_cfg [ par1 val1 [ par2 val2 [ ... ] ] ]" << std::endl;
  return( 1 );
  }

 if( std::string( argv[ 1 ] ) == std::string( argv[ 2 ] ) ) {
  std::cerr << "Error: in_cfg must be different from out_cfg " << std::endl;
  return( 1 );
  }

 // open input file
 std::ifstream ifile( argv[ 1 ] );
 if( ! ifile.is_open() ) {
  std::cerr << "Error: cannot open input file " << argv[ 1 ] << std::endl;
  return( 1 );
  }

 // open output file
 std::ofstream ofile( argv[ 2 ] , std::ofstream::out | std::ofstream::trunc );
 if( ! ofile.is_open() ) {
  std::cerr << "Error: cannot open output file " << argv[ 2 ] << std::endl;
  return( 1 );
  }

 // prepare vector of < parameter , value > pairs
 int npars = ( argc / 2 ) - 1;
 std::vector< std::pair< std::string , std::string > > pairs( npars );
 for( int i = 0 ; i < npars ; ++i ) {
  pairs[ i ].first = std::string( argv[ i * 2 + 3 ] );
  pairs[ i ].second = std::string( argv[ i * 2 + 4 ] );
  }

 // main loop - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 while( ! ifile.eof() ) {
  std::string line;
  getline( ifile , line );
  if( ( ! ifile.eof() ) && ifile.fail() ) {
   std::cerr << "Error reading from input file " << argv[ 1 ] << std::endl;
   return( 1 );
   }

  // skip initial whitespaces
  auto it = line.begin();
  while( ( it != line.end() ) && std::isspace( *it ) )
   ++it;

  if( it == line.end() ) {  // empty (or all-whitespace) line
   ofile << std::endl;
   continue;
   }

  if( *it == '#' ) {  // comments-only line
   #if ! BAREBONES
    ofile << line << std::endl;
   #endif
   continue;
   }

  // erase initial whitespace part, if any
  if( it != line.begin() )
   line.erase( line.begin() , it );

  // check if this is one of the parameters to be replaced
  auto pit = pairs.begin();
  for( ; pit != pairs.end() ; ++pit )
   if( line.find( (*pit).first ) != std::string::npos )
    break;

  if( pit != pairs.end() ) {  // found
   ofile << (*pit).first << "   " << (*pit).second;
   #if BAREBONES
    ofile << std::endl;
   #else
    for( it = line.begin() ; it != line.end() ; ++it )
     if( *it == '#' ) {
      line.erase( line.begin() , it );
      ofile << "   " << line << std::endl;
      break;
      }
   #endif
   pairs.erase( pit );
   if( pairs.empty() )
    break;
   }
  else {  // not found
   #if BAREBONES
    for( it = line.begin() ; it != line.end() ; ++it )
     if( *it == '#' ) {
      line.erase( it , line.end() );
      break;
      }
   #endif
   ofile << line << std::endl;
   } 
  }  // end( main loop )

 // cleanup loop: just copy the remaining part- - - - - - - - - - - - - - -
 while( ! ifile.eof() ) {
  std::string line;
  getline( ifile , line );
  if( ( ! ifile.eof() ) && ifile.fail() ) {
   std::cerr << "Error reading from input file " << argv[ 1 ] << std::endl;
   return( 1 );
   }
  #if BAREBONES
   auto it = line.begin();
   while( ( it != line.end() ) && std::isspace( *it ) )
    ++it;

   if( it == line.end() ) {  // empty (or all-whitespace) line
    ofile << std::endl;
    continue;
    }

   if( *it == '#' )  // comments-only line
    continue;

   for( ; it != line.end() ; ++it )
    if( *it == '#' ) {
     line.erase( it , line.end() );
     break;
     }
  #endif
  ofile << line << std::endl;
  }

 // the end
 ofile.close();  // may not be not necessary
 ifile.close();  // likely the destructor does this anyway
 
 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*----------------------- End File chgcfg.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
