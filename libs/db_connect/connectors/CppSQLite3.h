/*
 * CppSQLite
 * Developed by Rob Groves <rob.groves@btinternet.com>
 * Maintained by NeoSmart Technologies <http://neosmart.net/>
 * See LICENSE file for copyright and license info
 */

#ifndef _CppSQLite3_H_
#define _CppSQLite3_H_

#include <cstdio>
#include <cstring>
#include <iostream>
#include <sqlite3.h>
#include <string>

#define CPPSQLITE_ERROR 1000


class SQL_BLOB
{
  public:
    /// @note class must take case of deallocation itself!
    virtual const unsigned char* toBlob( ) const
    {
        return nullptr;
    };

    virtual const size_t blobSize( ) const
    {
        return 0;
    };

    virtual void fromBlob( const unsigned char*, const size_t )
    {}

    void fromPyBytesBlob( const std::string sStr )
    {
        fromBlob( (const unsigned char*)sStr.c_str( ), sStr.size( ) );
    }
}; // class

std::ostream& operator<<( std::ostream& os, const SQL_BLOB& );

class CppSQLite3Exception : public std::exception
{
  public:
    CppSQLite3Exception( const int nErrCode, const char* szErrMess, bool bDeleteMsg = true );

    CppSQLite3Exception( const CppSQLite3Exception& e );

    virtual ~CppSQLite3Exception( );

    const int errorCode( )
    {
        return mnErrCode;
    }

    const char* errorMessage( ) const
    {
        return mpszErrMess;
    }

    virtual const char* what( ) const throw( )
    {
        return errorMessage( );
    } // method

    static const char* errorCodeAsString( int nErrCode );

  private:
    int mnErrCode;
    char* mpszErrMess;
};

#if 0
class CppSQLite3Buffer
{
  public:
    CppSQLite3Buffer( );

    ~CppSQLite3Buffer( );

    const char* format( const char* szFormat, ... );

    operator const char*( )
    {
        return mpBuf;
    }

    void clear( );

  private:
    char* mpBuf;
};
#endif

class CppSQLite3Binary
{
  public:
    CppSQLite3Binary( );

    ~CppSQLite3Binary( );

    void setBinary( const unsigned char* pBuf, int nLen );
    void setEncoded( const unsigned char* pBuf );

    const unsigned char* getEncoded( );
    const unsigned char* getBinary( );

    int getBinaryLength( );

    unsigned char* allocBuffer( int nLen );

    void clear( );

  private:
    unsigned char* mpBuf;
    int mnBinaryLen;
    int mnBufferLen;
    int mnEncodedLen;
    bool mbEncoded;
};


class CppSQLite3Query
{
  public:
    CppSQLite3Query( );

    CppSQLite3Query( const CppSQLite3Query& rQuery );

    CppSQLite3Query( sqlite3* pDB, sqlite3_stmt* pVM, bool bEof, bool bOwnVM = true );

    CppSQLite3Query& operator=( const CppSQLite3Query& rQuery );

    virtual ~CppSQLite3Query( );

    int numFields( );

    int fieldIndex( const char* szField );
    const char* fieldName( int nCol );

    const char* fieldDeclType( int nCol );
    int fieldDataType( int nCol );

    const char* fieldValue( int nField );
    const char* fieldValue( const char* szField );

    int getIntField( int nField, int nNullValue = 0 );
    int getIntField( const char* szField, int nNullValue = 0 );

    long long getInt64Field( int nField, long long nNullValue = 0 );
    long long getInt64Field( const char* szField, long long nNullValue = 0 );

    double getFloatField( int nField, double fNullValue = 0.0 );
    double getFloatField( const char* szField, double fNullValue = 0.0 );

    const char* getStringField( int nField, const char* szNullValue = "" );
    const char* getStringField( const char* szField, const char* szNullValue = "" );

    const unsigned char* getBlobField( int nField, int& nLen );
    const unsigned char* getBlobField( const char* szField, int& nLen );

    bool fieldIsNull( int nField );
    bool fieldIsNull( const char* szField );

    bool eof( );

    void nextRow( );

    void finalize( );

  private:
    void checkVM( );

    sqlite3* mpDB;
    sqlite3_stmt* mpVM;
    bool mbEof;
    int mnCols;
    bool mbOwnVM;
};


class CppSQLite3Table
{
  public:
    CppSQLite3Table( );

    CppSQLite3Table( const CppSQLite3Table& rTable );

    CppSQLite3Table( char** paszResults, int nRows, int nCols );

    virtual ~CppSQLite3Table( );

    CppSQLite3Table& operator=( const CppSQLite3Table& rTable );

    int numFields( );

    int numRows( );

    const char* fieldName( int nCol );

    const char* fieldValue( int nField );
    const char* fieldValue( const char* szField );

    int getIntField( int nField, int nNullValue = 0 );
    int getIntField( const char* szField, int nNullValue = 0 );

    double getFloatField( int nField, double fNullValue = 0.0 );
    double getFloatField( const char* szField, double fNullValue = 0.0 );

    const char* getStringField( int nField, const char* szNullValue = "" );
    const char* getStringField( const char* szField, const char* szNullValue = "" );

    bool fieldIsNull( int nField );
    bool fieldIsNull( const char* szField );

    void setRow( int nRow );

    void finalize( );

  private:
    void checkResults( );

    int mnCols;
    int mnRows;
    int mnCurrentRow;
    char** mpaszResults;
};


class CppSQLite3Statement
{
  public:
    CppSQLite3Statement( );

    CppSQLite3Statement( const CppSQLite3Statement& rStatement );

    CppSQLite3Statement( sqlite3* pDB, sqlite3_stmt* pVM );

    virtual ~CppSQLite3Statement( );

    CppSQLite3Statement& operator=( const CppSQLite3Statement& rStatement );

    int execDML( );

    CppSQLite3Query execQuery( );

    void bind( int nParam, const char* szValue );
    void bind( int nParam, const std::string& szValue ); // inserted Arne 19/03/2016
    void bind( int nParam, const int nValue );
    void bind( int nParam, const long nValue );
    void bind( int nParam, const unsigned int nValue )
    {
        bind( nParam, (const long)nValue );
    } // method
    void bind( int nParam, const long long nValue );
    void bind( int nParam, const unsigned long nValue )
    {
        bind( nParam, (const long long)nValue );
    } // method
    void bind( int nParam, const double dwValue );
    // void bind( int nParam, const unsigned char* blobValue, int nLen );
    void bind( int nParam, const SQL_BLOB& rBlob );
    void bindNull( int nParam );

    void reset( );

    void finalize( );

  private:
    void checkDB( );
    void checkVM( );

    sqlite3* mpDB;
    sqlite3_stmt* mpVM;
};


class CppSQLite3DB
{
  public:
    CppSQLite3DB( );

    virtual ~CppSQLite3DB( );

    void open( const char* szFile );

    void close( );

    bool tableExists( const char* szTable );

    int execDML( const char* szSQL );

    CppSQLite3Query execQuery( const char* szSQL );

    int execScalar( const char* szSQL );

    CppSQLite3Table getTable( const char* szSQL );

    CppSQLite3Statement compileStatement( const char* szSQL );

    sqlite_int64 lastRowId( );

    void interrupt( )
    {
        sqlite3_interrupt( mpDB );
    } // method

    void setBusyTimeout( int nMillisecs );

    static const char* SQLiteVersion( )
    {
        return SQLITE_VERSION;
    }

    void set_num_threads(const int uiNumThreads);

    int loadOrSaveDb( const char* zFilename, int isSave );

    const char* getErrorMessage()
    {
        return sqlite3_errmsg(mpDB);
    }

  private:
    CppSQLite3DB( const CppSQLite3DB& db );

    CppSQLite3DB& operator=( const CppSQLite3DB& db );

    int mnBusyTimeoutMs;

    sqlite3_stmt* compile_v2( const char* szSQL );

  protected:
    void checkDB( );

    sqlite3* mpDB;
};

#endif
