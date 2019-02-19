/**
 * @file fileReader.h
 * @brief Reads queries from a file.
 * @author Markus Schmidt
 */
#ifndef FILE_READER_H
#define FILE_READER_H

#include "container/nucSeq.h"
#include "module/module.h"
#include "util/support.h"


#ifdef WITH_ZLIB
#include "zlib.h"
#endif


/// @cond DOXYGEN_SHOW_SYSTEM_INCLUDES
#include <fstream>
/// @endcond

namespace libMA
{

class FileStream
{
  public:
    DEBUG( size_t uiNumLinesRead = 0; ) // DEBUG

    FileStream( )
    {}

    FileStream( const FileStream& ) = delete;

    virtual bool eof( ) const
    {
        throw AnnotatedException( "Unimplemented" );
    }

    virtual bool is_open( ) const
    {
        throw AnnotatedException( "Unimplemented" );
    }
    virtual void close( )
    {
        throw AnnotatedException( "Unimplemented" );
    }
    virtual size_t tellg( )
    {
        throw AnnotatedException( "Unimplemented" );
    }
    virtual char peek( )
    {
        throw AnnotatedException( "Unimplemented" );
    }
    virtual void safeGetLine( std::string& t )
    {
        throw AnnotatedException( "Unimplemented" );
    }
}; // class

class StdFileStream : public FileStream
{
  private:
    std::ifstream xStream;

  public:
    StdFileStream( std::string sFilename ) : xStream( sFilename )
    {} // constructor

    bool eof( ) const
    {
        return !xStream.good( ) || xStream.eof( );
    } // method

    ~StdFileStream( )
    {
        DEBUG( std::cout << "StdFileStream read " << uiNumLinesRead << " lines in total." << std::endl;
               if( !eof( ) ) std::cerr << "WARNING: Did abort before end of File." << std::endl; ) // DEBUG
    } // deconstrucotr

    bool is_open( ) const
    {
        return xStream.is_open( );
    } // method

    void close( )
    {
        xStream.close( );
    } // method

    char peek( )
    {
        return xStream.peek( );
    } // method

    size_t tellg( )
    {
        return xStream.tellg( );
    } // method

    /**
     * code taken from
     * https://stackoverflow.com/questions/6089231/getting-std-ifstream-to-handle-lf-cr-and-crlf
     * suports windows linux and mac line endings
     */
    void safeGetLine( std::string& t )
    {
        t.clear( );
        DEBUG( uiNumLinesRead++; ) // DEBUG

        // The characters in the stream are read one-by-one using a std::streambuf.
        // That is faster than reading them one-by-one using the std::istream.
        // Code that uses streambuf this way must be guarded by a sentry object.
        // The sentry object performs various tasks,
        // such as thread synchronization and updating the stream state.

        std::istream::sentry se( xStream, true );
        std::streambuf* sb = xStream.rdbuf( );

        while( true )
        {
            int c = sb->sbumpc( );
            switch( c )
            {
                case '\n':
                    return;
                case '\r':
                    if( sb->sgetc( ) == '\n' )
                        sb->sbumpc( );
                    return;
                case std::streambuf::traits_type::eof( ):
                    // Also handle the case when the last line has no line ending
                    if( t.empty( ) )
                        xStream.setstate( std::ios::eofbit );
                    return;
                default:
                    t += (char)c;
            } // switch
        } // while
    } // method
}; // class

#ifdef WITH_ZLIB
class GzFileStream : public FileStream
{
  private:
    // gzFile is a pointer type
    gzFile pFile;
    int lastReadReturn = 1; // 1 == last read was ok; 0 == eof; -1 == error
    char cBuff;

  public:
    GzFileStream( std::string sFilename )
    {
        // open the file in reading and binary mode
        pFile = gzopen( sFilename.c_str( ), "rb" );
        if( pFile != nullptr )
            // fill internal buffer
            lastReadReturn = gzread( pFile, &cBuff, 1 );
    } // constructor

    bool is_open( ) const
    {
        return pFile != nullptr;
    } // method

    bool eof( ) const
    {
        return lastReadReturn != 1;
    } // method

    ~GzFileStream( )
    {
        DEBUG( std::cout << "GzFileStream read " << uiNumLinesRead << " lines in total." << std::endl;
               if( !eof( ) ) std::cerr << "WARNING: Did abort before end of File." << std::endl; ) // DEBUG
    } // deconstrucotr

    void close( )
    {
        gzclose( pFile );
    } // method

    size_t tellg( )
    {
        return gzoffset( pFile );
    } // method

    char peek( )
    {
        return cBuff;
    } // method

    void safeGetLine( std::string& t )
    {
        t.clear( );
        DEBUG( uiNumLinesRead++; ) // DEBUG

        while( true )
        {
            // gzread returns the number of bytes read; if we do not get 1 here something went wrong
            if( lastReadReturn != 1 )
                break;
            // if we reached the end of the line return
            if( cBuff == '\r' )
                // read the \n that should be the next character
                lastReadReturn = gzread( pFile, &cBuff, 1 );
            if( cBuff == '\n' )
                break;
            t += cBuff;

            // read one char from the stream
            lastReadReturn = gzread( pFile, &cBuff, 1 );
        } // while

        // read one char from the stream
        lastReadReturn = gzread( pFile, &cBuff, 1 );
    } // method
}; // class
#endif

class Reader
{
  public:
    virtual size_t getCurrPosInFile( ) const = 0;
    virtual size_t getFileSize( ) const = 0;
    virtual size_t getCurrFileIndex( ) const = 0;
    virtual size_t getNumFiles( ) const = 0;
}; // class

/**
 * @brief Reads Queries from a file.
 * @details
 * Reads (multi-)fasta or fastaq format.
 */
class FileReader : public Module<NucSeq, true>, public Reader
{
  public:
    std::shared_ptr<FileStream> pFile;
    size_t uiFileSize = 0;
    DEBUG( size_t uiNumLinesWithNs = 0; ) // DEBUG

  private:
    void construct( std::string sFileName )
    {
#ifdef WITH_ZLIB
        if( sFileName.size( ) > 3 && sFileName.substr( sFileName.size( ) - 3, 3 ) == ".gz" )
            pFile = std::make_shared<GzFileStream>( sFileName );
        else
#endif
            pFile = std::make_shared<StdFileStream>( sFileName );
        if( !pFile->is_open( ) )
        {
            throw AnnotatedException( "Unable to open file" + sFileName );
        } // if
        std::ifstream xFileEnd( sFileName, std::ifstream::ate | std::ifstream::binary );
        uiFileSize = xFileEnd.tellg( );
        if( uiFileSize == 0 )
            std::cerr << "Warning: empty file: " << sFileName << std::endl;
    }

  public:
    /**
     * @brief creates a new FileReader.
     */
    FileReader( std::string sFileName )
    {
        construct( sFileName );
    } // constructor
    /**
     * @brief creates a new FileReader.
     */
    FileReader( const ParameterSetManager& rParameters, std::string sFileName )
    {
        construct( sFileName );
    } // constructor

    ~FileReader( )
    {
        pFile->close( );
    } // deconstructor

    std::shared_ptr<NucSeq> EXPORTED execute( );

    // @override
    virtual bool requiresLock( ) const
    {
        return true;
    } // function

    size_t getCurrPosInFile( ) const
    {
        if( pFile->eof( ) )
            return uiFileSize;
        return pFile->tellg( );
    } // function

    size_t getFileSize( ) const
    {
        // prevent floating point exception here (this is only used for progress bar...)
        if( uiFileSize == 0 )
            return 1;
        return uiFileSize;
    } // function

    size_t getCurrFileIndex( ) const
    {
        return 0;
    } // method

    size_t getNumFiles( ) const
    {
        return 1;
    } // method
}; // class

class FileListReader : public Module<NucSeq, true>, public Reader
{
  public:
    std::vector<std::string> vsFileNames;
    FileReader xFileReader;
    size_t uiFileIndex = 0;

    inline void openNextFile( )
    {
        uiFileIndex++;
        if( uiFileIndex >= vsFileNames.size( ) )
            this->setFinished( );
        else
            xFileReader = FileReader( vsFileNames[ uiFileIndex ] );
    } // method

    /**
     * @brief creates a new FileReader.
     */
    FileListReader( std::vector<std::string> vsFileNames )
        : vsFileNames( vsFileNames ), xFileReader( vsFileNames[ uiFileIndex ] )
    {} // constructor

    FileListReader( const ParameterSetManager& rParameters, std::vector<std::string> vsFileNames )
        : vsFileNames( vsFileNames ), xFileReader( vsFileNames[ uiFileIndex ] )
    {} // constructor

    std::shared_ptr<NucSeq> EXPORTED execute( )
    {
        if( xFileReader.isFinished( ) )
            openNextFile( );
        return xFileReader.execute( );
    } // method

    // @override
    virtual bool requiresLock( ) const
    {
        return xFileReader.requiresLock( );
    } // function

    size_t getCurrPosInFile( ) const
    {
        return xFileReader.getCurrPosInFile( );
    } // function

    size_t getFileSize( ) const
    {
        return xFileReader.getFileSize( );
    } // function

    size_t getCurrFileIndex( ) const
    {
        return uiFileIndex;
    } // method

    size_t getNumFiles( ) const
    {
        return vsFileNames.size( );
    } // method
}; // class

typedef ContainerVector<std::shared_ptr<NucSeq>> TP_PAIRED_READS;
/**
 * @brief Reads Queries from a file.
 * @details
 * Reads (multi-)fasta or fastaq format.
 */
class PairedFileReader : public Module<TP_PAIRED_READS, true>, public Reader
{
  public:
    FileListReader xF1;
    FileListReader xF2;

    /**
     * @brief creates a new FileReader.
     */
    PairedFileReader( const ParameterSetManager& rParameters,
                      std::vector<std::string>
                          vsFileName1,
                      std::vector<std::string>
                          vsFileName2 )
        : xF1( vsFileName1 ), xF2( vsFileName2 )
    {} // constructor

    std::shared_ptr<TP_PAIRED_READS> EXPORTED execute( );

    // @override
    virtual bool requiresLock( ) const
    {
        return xF1.requiresLock( ) || xF2.requiresLock( );
    } // function

    size_t getCurrPosInFile( ) const
    {
        return xF1.getCurrPosInFile( ) + xF2.getCurrPosInFile( );
    } // function

    size_t getFileSize( ) const
    {
        return xF1.getFileSize( ) + xF2.getFileSize( );
    } // function

    size_t getCurrFileIndex( ) const
    {
        return xF1.getCurrFileIndex( ) + xF2.getCurrFileIndex( );
    } // method

    size_t getNumFiles( ) const
    {
        return xF1.getNumFiles( ) + xF2.getNumFiles( );
    } // method
}; // class

} // namespace libMA

#ifdef WITH_PYTHON
#ifdef WITH_BOOST
void exportFileReader( );
#else
void exportFileReader( py::module& rxPyModuleId );
#endif
#endif

#endif