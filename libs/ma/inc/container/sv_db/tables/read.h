/**
 * @file sequencer.h
 * @details
 * Database interface for the structural variant caller.
 * One table of the database.
 */
#pragma once

#include "util/sqlite3.h"

namespace libMA
{

typedef CppSQLiteExtTableWithAutomaticPrimaryKey<int64_t, // sequencer id (foreign key)
                                                 std::string, // read name
                                                 NucSeqSql // read sequence
                                                 >
    TP_READ_TABLE;
class ReadTable : public TP_READ_TABLE
{
    std::shared_ptr<CppSQLiteDBExtended> pDatabase;
    bool bDoDuplicateWarning = true;

  public:
    CppSQLiteExtQueryStatement<int32_t> xGetReadId;
    CppSQLiteExtQueryStatement<NucSeqSql> xGetRead;

    ReadTable( std::shared_ptr<CppSQLiteDBExtended> pDatabase )
        : TP_READ_TABLE( *pDatabase, // the database where the table resides
                         "read_table", // name of the table in the database
                         // column definitions of the table
                         std::vector<std::string>{"sequencer_id", "name", "sequence"},
                         // constraints for table
                         std::vector<std::string>{"FOREIGN KEY (sequencer_id) REFERENCES sequencer_table(id) "} ),
          pDatabase( pDatabase ),
          xGetReadId( *pDatabase, "SELECT id FROM read_table WHERE sequencer_id == ? AND name == ? " ),
          xGetRead( *pDatabase, "SELECT sequence FROM read_table WHERE id == ? " )
    {} // default constructor

    inline int64_t insertRead( int64_t uiSequencerId, std::shared_ptr<NucSeq> pRead )
    {
        return xInsertRow( uiSequencerId, pRead->sName, NucSeqSql( pRead ) );
    } // method

    inline std::shared_ptr<NucSeq> getRead( int64_t iId )
    {
        auto xRes = xGetRead.scalar( iId );
        xRes.pNucSeq->iId = iId;
        return xRes.pNucSeq;
    } // method
}; // class

} // namespace libMA