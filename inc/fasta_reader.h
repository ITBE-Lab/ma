#pragma once

#include <memory>
#include <iostream>
#include <array>
#include "support.h"
#include "sequence.h"

// #define USE_STL

#ifdef USE_STL
	typedef std::string TextSequence;
#endif

/* Structure that describes single FASTA Record.
 */
class FastaDescriptor
{
public :
	std::string sName;	// name of the FASTA Record
	std::string sComment; // comment for the entry
	TextSequence qualifier; // qualifier (describes quality of a read.)

	/* Here we take a text sequence because FASTA files contain text.
	 */
	std::unique_ptr<TextSequence> pSequenceRef; 

	FastaDescriptor() 
		: sName(), sComment(), qualifier(), pSequenceRef( new TextSequence() ) // std::make_shared<TextSequence>() )
	{ } // default constructor
}; // class

/* StreamType can be GzipInputStream or GzipInputFileStream.
 */
template<int BUFFER_SIZE, typename StreamType>
class BufferedStreamer
{
protected :
	/* The stream used for all reading operations.
	 * FIX ME: The position of the stream over here is a design flaw.
	 */
	StreamType xBufferedStream;

private :
	/* We prohibit the copy of Objects of this class.
	 */
	BufferedStreamer<BUFFER_SIZE, StreamType>(const BufferedStreamer&);

	/* The buffer is placed on the heap in order to safe stack space.
	 */
	std::array<char, BUFFER_SIZE> aBuffer;

	/* References to the first and last element in the buffer
	 */
	size_t uiBegin = 0; 
	size_t uiEnd = 0;

	/* bIsEOF becomes true, if we reached the last block
	 */
	bool bIsEOF = false;   

	inline void vRefillBuffer()
	{
		uiBegin = 0;   

		/* Fill the buffer by reading from the stream.
		 */
		xBufferedStream.read( &aBuffer[0], BUFFER_SIZE );

		if ( xBufferedStream.bad() )
		{
			throw fasta_reader_exception( "Something went wrong during FASTA stream reading" );
		} // if

		/* uiEnd saves the number of characters that we could read in the stream operation
		 */
		uiEnd = (size_t)xBufferedStream.gcount();

		if (uiEnd < BUFFER_SIZE)
		{
			bIsEOF = 1; 
		} // if
	} // inline method

public :
	/* Constructor:
	 * rxBufferedStreamConstructorArg is simply forwarded to the constructor of the buffered stream.
	 */
	template<typename TP>
	BufferedStreamer<BUFFER_SIZE, StreamType>( TP &rxBufferedStreamConstructorArg /* const char* pcFileNameRef */ ) :
	xBufferedStream( rxBufferedStreamConstructorArg /* pcFileNameRef */ ),
		uiBegin( 0 ), 
		uiEnd( 0 ), 
		bIsEOF( false )
	{} // constructor

	/* Virtual destructor. (interesting in the context of polymorphism.)
	 */
	virtual ~BufferedStreamer()
	{} // destructor

	/* keep me here, so that I stay automatically inlined ...
	 */
	inline bool getSingleChar( char &cChar )                               
	{                                                                                                            
		if (uiBegin >= uiEnd) 
		{                                                   
			if ( bIsEOF == false )
			{
				vRefillBuffer();

				if (uiEnd == 0 )
				{
					return false;
				} // if level 3
			} // if level 2
			else
			{
				return false;
			} // else
		} // if level 1

		cChar = aBuffer[uiBegin++];

		return true;                                      
	} // inline method

	/* Reads from the stream until it sees the given delimiter.
	 * Returns: true  : could find some string until delimiter
	 *          false : otherwise
	 */
	inline bool bGetUntilDelimiter( const bool bUntil_CR_or_LF, TextSequence &rSequence, char &rcLastCharacterRead ) 
	{                                                                                                                                       
		rcLastCharacterRead = '\0';  

		while ( true ) 
		{                                                                                                              
			size_t uiIterator;   

			if (uiBegin >= uiEnd) 
			{                                                                     
				/* We consumed the complete buffer content
				 */
				if ( bIsEOF == false )
				{                                                                              
					vRefillBuffer();
					
					if ( uiEnd == 0 ) 
					{
						/* We reached the end of file and couldn't read anything.
						 */
						return false; 
					}
				}  // if
				else
				{
					/* The buffer is empty and we have an EOF, so we exit the while loop and return
					 */
					return false;  
				} // else
			} // if                                                                                                                

			if ( bUntil_CR_or_LF ) 
			{                                                                                        
				/* We read until we see a CR or LF
				 */
				for ( uiIterator = uiBegin; uiIterator < uiEnd; ++uiIterator ) 
				{
					if ( aBuffer[uiIterator] == '\n' || aBuffer[uiIterator] == '\r' ) 
					{
						break;
					} // if
				} // for			
			} // if
			else 
			{                                                                                                       
				/* We read until we see some space 
				 */
				for( uiIterator = uiBegin; uiIterator < uiEnd; ++uiIterator )
				{
					if ( isspace( static_cast<unsigned char>( aBuffer[uiIterator] ) ) )  // CHECK: Is it possible to work with a reinterpret_cast over here?
						break;
				} // for
			}   
#ifdef USE_STL
			rSequence.append( aBuffer + uiBegin, uiIterator - uiBegin ); 
#else
			rSequence.PlainSequence<char>::vAppend( &aBuffer[0] + uiBegin, uiIterator - uiBegin );
#endif
			uiBegin = uiIterator + 1; 

			if( uiIterator < uiEnd ) 
			{
				/* We found the requested delimiters or spaces.
				 * We store the last character that we got in cLastCharacterRead
				 */
				rcLastCharacterRead = aBuffer[uiIterator];
				
				break;                                                                                                
			} // outer if                                                                                                                
		} // while                                                                                                                             

		return true;                                                                                                 
	} // method
}; // class

/* A FastaStreamReader can read a FASTA-file and creates the appropriate FASTA record.
 * Requires as template argument a stream type:
 * E.g.: std::istream, std::ifstream, GzipInputFileStream, GzipInputStream
 * FIX ME: Rethink the design of the FASTA reader. The current version is not really well made.
 */
template<typename StreamType>
class FastaStreamReader : public BufferedStreamer<8192, StreamType>
{ 
private:
	/* Indicates internally, whether the currently processed sequence is the first one.
	 */
	bool bIsFirstSequence = true;

	/* Return values:
	   >=0  length of the sequence (more sequences following)
	    0 : read a sequence no more sequences following
		1 : There are more sequences following
	   -1   reading failed
	   -2   truncated quality string
	 */                                                                                                           
	int readFastaRecord( FastaDescriptor &fastaRecord ) //, bool bIsFirstSequence )                                                                      
	{                                                                                                                                       
		char cChar;     

		if ( bIsFirstSequence ) 
		{ 
			/* then jump to the next header line 
			 */ 
			while ( true )
			{
				if ( BufferedStreamer<8192, StreamType>::getSingleChar( cChar ) == false )
				{
					return -1;
				} // if
				
				if ( cChar == '>' || cChar == '@' )
				{
					/* We did see the beginning of a FASTA record. 
					 */
					break;
				} // if
			} // while   
			bIsFirstSequence = false;
		} // if    

		/* We read the name. (String until the first space / new line)
		 */
		TextSequence bufferSequence;
		if ( BufferedStreamer<8192, StreamType>::bGetUntilDelimiter( false, bufferSequence, cChar ) == false )
		{
			return -1;
		} // if

#ifdef USE_STL
		fastaRecord.sName = bufferSequence; 
		bufferSequence.clear();
#else
		fastaRecord.sName = bufferSequence.cString();
		bufferSequence.vClear();
#endif

		/* We read the remaining part of the first line as comment
		 */
		if ( cChar != '\n' && cChar != '\r' )
		{
			if ( BufferedStreamer<8192, StreamType>::bGetUntilDelimiter( true, bufferSequence, cChar ) == false )
			{
				return -1;
			} // inner if
		} // if

#ifdef USE_STL
		fastaRecord.sComment = bufferSequence; 
		bufferSequence.clear();
#else
		/* TO DO: We have to trim the comment, because there can be LF inside in some file formats.
		 */
		fastaRecord.sComment = bufferSequence.cString();
		bufferSequence.vClear();
#endif
	
		/* We read the core sequence
		 */
		while (	true ) 
		{ 
			if ( BufferedStreamer<8192, StreamType>::getSingleChar( cChar ) == false )
			{
				/* EOF there isn't anything more ...
				 */
				return 0;
			} // if

			if ( (cChar == '>') || (cChar == '@') )
			{
				/* We found the beginning of a next sequence
				 * '>' == FASTA, '@' == FASTQ
				 */
				return 1;
			} // if

			if ( cChar == '+' )
			{
				/* We found a qualifier ...
				 */
				break;
			}
			
			if ( isgraph( cChar ) ) 
			{ 
				/* We found a printable non-space character and 
				 * append the single character to the sequence
				 */
#ifdef USE_STL
				fastaRecord.pSequenceRef->push_back( cChar ); 
#else
				fastaRecord.pSequenceRef->vAppend( cChar );
#endif
			} // if                                                                                                                 
		} // while
	    
		/* skip the rest of the '+' line 
		 */
#ifdef USE_STL
		bufferSequence.clear(); // <- bufferSequence.vClear();
#else
		bufferSequence.vClear();
#endif
		BufferedStreamer<8192, StreamType>::bGetUntilDelimiter( true, bufferSequence, cChar );

		/* TO OD; Fore a reserve for qualifier using the size of the sequence
		 */
		while (	true ) 
		{ 
			if ( BufferedStreamer<8192, StreamType>::getSingleChar( cChar ) == false )
			{
				/* EOF there isn't anything more ...
				 */
				break;
			} // if

			if ( cChar >= 33 && cChar <= 127 ) 
			{
#ifdef USE_STL
				fastaRecord.qualifier.push_back( cChar ); 
#else
				fastaRecord.qualifier.vAppend( cChar );
#endif
			} // if
		} // while

#ifdef USE_STL		
		if ( fastaRecord.pSequenceRef->length() != fastaRecord.qualifier.length() ) // <- if ( fastaRecord.pSequenceRef->uxGetSequenceSize() != fastaRecord.qualifier.uxGetSequenceSize() )
#else
		if ( fastaRecord.pSequenceRef->uxGetSequenceSize() != fastaRecord.qualifier.uxGetSequenceSize() )
#endif
		{
			return -2;
		}  // if  

		return 0;
	} //method

public :
	/* Apply the function func to all FASTA sequences defined in the given FASTA stream.
	 */
	template <typename FunctionType>
	void forAllSequencesDo( FunctionType &&function )
	{
		int iContinue = 1;

		while( iContinue > 0 )
		{
			FastaDescriptor xFastaRecord; // FIX ME: Analyze whether it is better to always reuse the same record and to clear instead.
			
			/* We read a single record.
			 */
			iContinue = readFastaRecord( xFastaRecord );
			function( xFastaRecord );
		} // while
	} // generic function

	/* Sequence provider gets access to the private stuff! 
	 */
	friend class FastaReader;

	/* Deprecated. Remove me.
	 */
	FastaStreamReader( std::istream &xInputStream, FastaDescriptor &fastaRecord ) : 
		BufferedStreamer<8192, StreamType>( xInputStream ), 
		bIsFirstSequence( true )
	{
		/* We read a single record.
		 */
		readFastaRecord( fastaRecord );
	} // constructor

	/* Constructor:
	 * The current design always creates a separated stream for all reading. 
	 * This stream is invisible to the outside and constructed as part of the buffer.
	 * TP must be: 
	 * std::istream for GzipInputStream (std::istream is the source for GzipInputStream)
	 * std::istream for std::istream	 
	 */
	template<typename TP> 
	FastaStreamReader( TP &rxBufferedStreamConstructorArg ) : 
		BufferedStreamer<8192, StreamType>( rxBufferedStreamConstructorArg )
	{} // constructor

	virtual ~FastaStreamReader()
	{
		/* Automatic call of the superclass destructor
		 */
	} // destructor	
}; // class

/* Use this a FASTA rader for file streams. (GzipInputFileStream and std::ifstream)
 * The constructor checks, whether the file could be successfully opened.
 */
template<typename StreamType>
class FastaFileStreamReader : public FastaStreamReader<StreamType>
{
public :
	/* The stream used for all reading operations.
	 */
	FastaFileStreamReader( const std::string &rsFileName ) :
		FastaStreamReader<StreamType>( rsFileName )
	{
		/* We check, whether file opening worked well.
		 */
		if ( !this->xBufferedStream.is_open() )
		{
			throw fasta_reader_exception( "File open error." );
		} // if
	} // constructor
}; // class FastaFileStreamReader

/* Reads a single FASTA record from a file
 */
class FastaReader : public FastaDescriptor
{                                                                                    
public :
	/* The default constructor.
	 */
	FastaReader()
	{ } 

	/* The central load function.
	 */
	void vLoadFastaFile( const char *pcFileName )
	{
		std::ifstream xFileInputStream( pcFileName, std::ios::in | std::ios::binary );
		
		/* We check, whether file opening worked well.
		 */
		if ( !xFileInputStream.is_open() )
		{
			throw fasta_reader_exception( "File open error." );
		} // if
		
		/* If the FASTA reader experiences some problem, it will throw an exception.
		 */
		FastaStreamReader<GzipInputStream> fastaReader( xFileInputStream, *this );
	} // method Anonymous

	/* The destruction will free the memory of the FASTA-Record
	 */
	~FastaReader()
	{ } // destructor
}; // class