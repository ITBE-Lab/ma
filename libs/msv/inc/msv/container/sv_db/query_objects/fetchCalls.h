/**
 * @file fetchCalls.h
 * @brief implements libMSV::SvCallsFromDb; a class that fetches libMSV::SvCall objects from a DB.
 * @author Markus Schmidt
 */
#pragma once

#include "ms/util/parameter.h"
#include "msv/container/sv_db/tables/svCall.h"
#include "msv/container/sv_db/tables/svCallSupport.h"
#include <bitset>

namespace libMSV
{
/**
 * @brief fetches libMSV::SvCall objects from a DB.
 * @details
 * Can do several 2d range queries.
 * @todo this should become a container
 */
template <typename DBCon> class SvCallsFromDb
{
    std::shared_ptr<DBCon> pConnection;
    // the following two tables are not used. However their constructors guarantee their existence and the correctness
    // of rows
    std::shared_ptr<SvCallTable<DBCon>> pSvCallTable;
    std::shared_ptr<SvCallSupportTable<DBCon>> pSvCallSupportTable;
    // rectangle for xQuery; stays uninitialized if unused
    WKBUint64Rectangle xWkb;
    SQLQuery<typename DBCon::SlaveType, uint32_t, uint32_t, uint32_t, uint32_t, bool, bool, bool, uint32_t,
             PriKeyDefaultType, PriKeyDefaultType>
        xQuerySupport;

    enum ConfigFlags
    {
        /// @brief only fetch calls in specific area
        InArea,
        /// @brief only fetch calls that overlap/do not overlap calls from different run (ground truth run)
        Overlapping,
        /// @brief true: fetch overlapping calls; false: fetch non overlapping calls
        WithIntersection,
        /// @brief true: do not fetch calls that are overlapped by higher scoring calls from same run
        WithSelfIntersection,
        /// @brief true: ignore ground truth run calls that are overlapped by higher scoring calls
        WithOtherIntersection,
        WithMinScore,
        WithMaxScore,
        WithMinScoreGT,
        WithMaxScoreGT,
        JustCount,
        WithAvgSuppNtRange,
        WithAvgSuppNtRangeGT,
        OrderByScore,
        Limit,
        ExcludeSpecificID,
        OnlyWithDummyJumps,
        WithoutDummyJumps,
        MinQueryDist,
        DUMMY_FOR_COUNT // must be last
    };
    std::bitset<ConfigFlags::DUMMY_FOR_COUNT> xConfiguration;

    std::unique_ptr<SQLQuery<DBCon, PriKeyDefaultType, uint32_t, uint32_t, uint32_t, uint32_t, bool, bool, NucSeqSql,
                             uint32_t, uint32_t, uint32_t>>
        pQuery = nullptr;
    std::unique_ptr<SQLQuery<DBCon, uint32_t>> pQueryCount = nullptr;

    std::string rectanglesOverlapSQL( std::string sFromTable, std::string sToTable )
    {
        // clang-format off
        // ST_DWithin returns true if the geometries are within the specified distance of one another
        // makes use of indices
        return  std::string("     AND ( (ST_DWithin(") + sToTable + ".rectangle::geometry, "
                            "                       " + sFromTable + ".rectangle::geometry, ?) "
                            "           AND " + sToTable + ".from_forward = " + sFromTable + ".from_forward "
                            "           AND " + sToTable + ".to_forward = " + sFromTable + ".to_forward) "
                            // this considers reverse complemented calls
                            // Note: we need to mirror the rectangle on the matrix diagonal
                            //       and invert the strand information
                            "       OR (ST_DWithin(" + sToTable + ".rectangle::geometry, "
                            "                      " + sFromTable + ".flipped_rectangle::geometry, ?) "
                            "           AND " + sToTable + ".from_forward != " + sFromTable + ".to_forward "
                            "           AND " + sToTable + ".to_forward != " + sFromTable + ".from_forward)"
                            "     )";
        // clang-format on
    }
    std::string selfIntersectionSQL( std::string sFromTable, std::string sToTable )
    {
        // clang-format off
        return std::string("AND NOT EXISTS( "
                        // make sure that inner_table does not overlap with any other call with higher score
                        "     SELECT " ) + sToTable + ".id "
                        "     FROM sv_call_table AS " + sToTable +
                        "     WHERE " + sToTable + ".id != " + sFromTable + ".id "
                        "     AND " + sToTable + ".score >= " + sFromTable + ".score "
                        "     AND " + sToTable + ".sv_caller_run_id = " + sFromTable + ".sv_caller_run_id "
                        // Returns true if the geometries are within the specified distance of one another
                        // makes use of indices
                        + rectanglesOverlapSQL(sFromTable, sToTable) +
                        ") ";
        // clang-format on
    }

    void initQuery( std::bitset<ConfigFlags::DUMMY_FOR_COUNT> xNewConfig )
    {
        if( xNewConfig != xConfiguration || ( xNewConfig[ ConfigFlags::JustCount ] && pQueryCount == nullptr ) ||
            ( !xNewConfig[ ConfigFlags::JustCount ] && pQuery == nullptr ) )
        {
            while( pQueryCount != nullptr && !pQueryCount->eof( ) )
                pQueryCount->next( );
            pQueryCount = nullptr;
            while( pQuery != nullptr && !pQuery->eof( ) )
                pQuery->next( );
            pQuery = nullptr;
            xConfiguration = xNewConfig;

            std::string sQueryText =
                std::string( "FROM sv_call_table AS inner_table "
                             "WHERE sv_caller_run_id = ? " ) +
                ( !xConfiguration[ ConfigFlags::ExcludeSpecificID ] ? "" : "AND id != ? " ) + //
                ( !xConfiguration[ ConfigFlags::MinQueryDist ]
                      ? ""
                      : "AND ? <= (SELECT AVG(sv_jump_table.query_to - sv_jump_table.query_from) "
                        "          FROM sv_jump_table "
                        "          JOIN sv_call_support_table ON sv_call_support_table.jump_id = sv_jump_table.id "
                        "          WHERE sv_call_support_table.call_id = inner_table.id "
                        "          ) " ) + //
                ( !xConfiguration[ ConfigFlags::InArea ] ? ""
                                                         : "AND " ST_INTERSCTS
                                                           "(rectangle, ST_GeomFromWKB(?, 0)) " ) + //
                ( !xConfiguration[ ConfigFlags::WithMinScore ] ? "" : "AND score >= ? " ) + //
                ( !xConfiguration[ ConfigFlags::WithMaxScore ] ? "" : "AND score < ? " ) + //
                ( !xConfiguration[ ConfigFlags::WithAvgSuppNtRange ] ? "" : "AND avg_supporting_nt >= ? " ) + //
                ( !xConfiguration[ ConfigFlags::WithAvgSuppNtRange ] ? "" : "AND avg_supporting_nt < ? " ) + //
                ( ( !xConfiguration[ ConfigFlags::OnlyWithDummyJumps ] &&
                    !xConfiguration[ ConfigFlags::WithoutDummyJumps ] )
                      ? ""
                      : std::string( "AND " ) + ( xConfiguration[ ConfigFlags::WithoutDummyJumps ] ? "NOT " : "" ) +
                            "EXISTS( "
                            "           SELECT sv_jump_table.id "
                            "           FROM sv_jump_table "
                            "           JOIN sv_call_support_table on sv_jump_table.id = sv_call_support_table.jump_id "
                            "           WHERE sv_call_support_table.call_id = inner_table.id "
                            "           AND (sv_jump_table.from_pos = ? OR sv_jump_table.to_pos = ?) "
                            "           ) " ) + //
                ( !xConfiguration[ ConfigFlags::WithIntersection ]
                      ? ""
                      : std::string( "AND " ) + ( xConfiguration[ ConfigFlags::Overlapping ] ? "" : "NOT " ) +
                            // make sure that inner_table overlaps the outer_table:
                            "EXISTS( "
                            "     SELECT outer_table.id "
                            "     FROM sv_call_table AS outer_table "
                            "     WHERE outer_table.sv_caller_run_id = ? " +
                            ( !xConfiguration[ ConfigFlags::WithMinScoreGT ] ? "" : "AND outer_table.score >= ? " ) + //
                            ( !xConfiguration[ ConfigFlags::WithMaxScoreGT ] ? "" : "AND outer_table.score < ? " ) + //
                            ( !xConfiguration[ ConfigFlags::WithAvgSuppNtRangeGT ]
                                  ? ""
                                  : "AND outer_table.avg_supporting_nt >= ? " ) + //
                            ( !xConfiguration[ ConfigFlags::WithAvgSuppNtRangeGT ]
                                  ? ""
                                  : "AND outer_table.avg_supporting_nt < ? " ) + //
                            rectanglesOverlapSQL( "outer_table", "inner_table" ) +
                            ( !xConfiguration[ ConfigFlags::WithOtherIntersection ]
                                  ? ""
                                  : selfIntersectionSQL( "outer_table", "outer_table2" ) ) +
                            ") " ) +
                ( !xConfiguration[ ConfigFlags::WithSelfIntersection ]
                      ? ""
                      : selfIntersectionSQL( "inner_table", "inner_table2" ) ) +
                ( !xConfiguration[ ConfigFlags::OrderByScore ] ? "" : "ORDER BY score DESC " ) +
                ( !xConfiguration[ ConfigFlags::Limit ] ? "" : "LIMIT ? " );

            if( xConfiguration[ ConfigFlags::JustCount ] )
                pQueryCount = std::make_unique<SQLQuery<DBCon, uint32_t>>(
                    pConnection, std::string( "SELECT COUNT(*) " ) + sQueryText );
            else
                pQuery = std::make_unique<SQLQuery<DBCon, PriKeyDefaultType, uint32_t, uint32_t, uint32_t, uint32_t,
                                                   bool, bool, NucSeqSql, uint32_t, uint32_t, uint32_t>>(
                    pConnection,
                    std::string( "SELECT id, from_pos, to_pos, from_size, to_size, from_forward, to_forward, "
                                 "       inserted_sequence, supporting_reads, reference_ambiguity, supporting_nt " ) +
                        sQueryText );
        };
    } // method

    template <typename... Types>
    void initFetchQuery_( std::bitset<ConfigFlags::DUMMY_FOR_COUNT> xNewConfig, Types... args )
    {
        initQuery( xNewConfig );
        pQuery->execAndFetch( args... );
    } // method

  public:
    SvCallsFromDb( std::shared_ptr<DBCon> pConnection )
        : pConnection( pConnection ),
          pSvCallTable( std::make_shared<SvCallTable<DBCon>>( pConnection ) ),
          pSvCallSupportTable( std::make_shared<SvCallSupportTable<DBCon>>( pConnection ) ),
          xQuerySupport( pConnection->getSlave( ),
                         "SELECT from_pos, to_pos, query_from, query_to, from_forward, to_forward, was_mirrored, "
                         "       num_supporting_nt, sv_jump_table.id, read_id "
                         "FROM sv_call_support_table "
                         "JOIN sv_jump_table ON sv_call_support_table.jump_id = sv_jump_table.id "
                         "WHERE sv_call_support_table.call_id = ? " )
    {}

    void initFetchQuery( int64_t iSvCallerIdA, int64_t iX, int64_t iY, int64_t iW, int64_t iH,
                         nucSeqIndex uiMinQueryDist )
    {
        xWkb = geom::Rectangle<nucSeqIndex>( std::max( iX, (int64_t)0 ), std::max( iY, (int64_t)0 ),
                                             std::max( iW, (int64_t)0 ), std::max( iH, (int64_t)0 ) );
        std::bitset<ConfigFlags::DUMMY_FOR_COUNT> xConfig;
        xConfig.set( ConfigFlags::InArea );
        xConfig.set( ConfigFlags::OrderByScore );
        xConfig.set( ConfigFlags::Limit );
        xConfig.set( ConfigFlags::WithoutDummyJumps );
        xConfig.set( ConfigFlags::MinQueryDist );
        initFetchQuery_( xConfig, iSvCallerIdA, uiMinQueryDist, xWkb, SvJump::DUMMY_LOCATION, SvJump::DUMMY_LOCATION,
                         1 );
    }

    void initFetchDummiesQuery( int64_t iSvCallerIdA )
    {
        std::bitset<ConfigFlags::DUMMY_FOR_COUNT> xConfig;
        xConfig.set( ConfigFlags::OnlyWithDummyJumps );
        initFetchQuery_( xConfig, iSvCallerIdA, SvJump::DUMMY_LOCATION, SvJump::DUMMY_LOCATION );
    }

    void initFetchQuery( int64_t iSvCallerIdA, int64_t iX, int64_t iY, int64_t iW, int64_t iH, int64_t iSvCallerIdB,
                         bool bOverlapping, int64_t iAllowedDist, double dMinScore, double dMaxScore )
    {
        xWkb = geom::Rectangle<nucSeqIndex>( std::max( iX, (int64_t)0 ), std::max( iY, (int64_t)0 ),
                                             std::max( iW, (int64_t)0 ), std::max( iH, (int64_t)0 ) );
        std::bitset<ConfigFlags::DUMMY_FOR_COUNT> xConfig;
        xConfig.set( ConfigFlags::InArea );
        xConfig.set( ConfigFlags::Overlapping, bOverlapping );
        xConfig.set( ConfigFlags::WithIntersection );
        xConfig.set( ConfigFlags::WithSelfIntersection );
        xConfig.set( ConfigFlags::WithMinScore );
        xConfig.set( ConfigFlags::WithMaxScore );
        initFetchQuery_(
            xConfig, iSvCallerIdA, xWkb, dMinScore, dMaxScore, iSvCallerIdB, iAllowedDist, iAllowedDist,
            // self intersection must be x2 other intersection to properly cancel out double true positive calls
            iAllowedDist * 2, iAllowedDist * 2 );
    }

    void initFetchQuery( double dMinScoreGT, double dMaxScoreGT, int64_t iSvCallerIdA, int64_t iX, int64_t iY,
                         int64_t iW, int64_t iH, int64_t iSvCallerIdB, bool bOverlapping, int64_t iAllowedDist )
    {
        xWkb = geom::Rectangle<nucSeqIndex>( std::max( iX, (int64_t)0 ), std::max( iY, (int64_t)0 ),
                                             std::max( iW, (int64_t)0 ), std::max( iH, (int64_t)0 ) );
        std::bitset<ConfigFlags::DUMMY_FOR_COUNT> xConfig;
        xConfig.set( ConfigFlags::InArea );
        xConfig.set( ConfigFlags::Overlapping, bOverlapping );
        xConfig.set( ConfigFlags::WithIntersection );
        xConfig.set( ConfigFlags::WithOtherIntersection );
        xConfig.set( ConfigFlags::WithMinScoreGT );
        xConfig.set( ConfigFlags::WithMaxScoreGT );
        initFetchQuery_(
            xConfig, iSvCallerIdA, xWkb, iSvCallerIdB, dMinScoreGT, dMaxScoreGT, iAllowedDist, iAllowedDist,
            // other intersection must be x2 other intersection to properly cancel out double true positive calls
            iAllowedDist * 2, iAllowedDist * 2 );
    }

    void initFetchQuery( int64_t iSvCallerIdA, int64_t iX, int64_t iY, int64_t iW, int64_t iH, int64_t iSvCallerIdB,
                         bool bOverlapping, int64_t iAllowedDist )
    {
        xWkb = geom::Rectangle<nucSeqIndex>( std::max( iX, (int64_t)0 ), std::max( iY, (int64_t)0 ),
                                             std::max( iW, (int64_t)0 ), std::max( iH, (int64_t)0 ) );
        std::bitset<ConfigFlags::DUMMY_FOR_COUNT> xConfig;
        xConfig.set( ConfigFlags::InArea );
        xConfig.set( ConfigFlags::Overlapping, bOverlapping );
        xConfig.set( ConfigFlags::WithIntersection );
        xConfig.set( ConfigFlags::WithSelfIntersection );
        initFetchQuery_(
            xConfig, iSvCallerIdA, xWkb, iSvCallerIdB, iAllowedDist, iAllowedDist,
            // self intersection must be x2 other intersection to properly cancel out double true positive calls
            iAllowedDist * 2, iAllowedDist * 2 );
    }

    void initFetchQuery( int64_t iSvCallerIdA, int64_t iX, int64_t iY, int64_t iW, int64_t iH, double dMinScore,
                         double dMaxScore )
    {
        xWkb = geom::Rectangle<nucSeqIndex>( std::max( iX, (int64_t)0 ), std::max( iY, (int64_t)0 ),
                                             std::max( iW, (int64_t)0 ), std::max( iH, (int64_t)0 ) );
        std::bitset<ConfigFlags::DUMMY_FOR_COUNT> xConfig;
        xConfig.set( ConfigFlags::InArea );
        xConfig.set( ConfigFlags::WithMinScore );
        xConfig.set( ConfigFlags::WithMaxScore );
        initFetchQuery_( xConfig, iSvCallerIdA, xWkb, dMinScore, dMaxScore );
    }

    void initFetchQuery( int64_t iSvCallerIdA, int64_t iX, int64_t iY, int64_t iW, int64_t iH )
    {
        xWkb = geom::Rectangle<nucSeqIndex>( std::max( iX, (int64_t)0 ), std::max( iY, (int64_t)0 ),
                                             std::max( iW, (int64_t)0 ), std::max( iH, (int64_t)0 ) );
        std::bitset<ConfigFlags::DUMMY_FOR_COUNT> xConfig;
        xConfig.set( ConfigFlags::InArea );
        initFetchQuery_( xConfig, iSvCallerIdA, xWkb );
    }

    ~SvCallsFromDb( )
    {
        while( pQueryCount != nullptr && !pQueryCount->eof( ) )
            pQueryCount->next( );
        while( pQuery != nullptr && !pQuery->eof( ) )
            pQuery->next( );
    } // destructor

    /**
     * @brief fetches the next call.
     * @details
     * behaviour is undefined if libMSV::SvCallsFromDb::hasNext returns false
     */
    SvCall next( bool bWithSupport = true )
    {
        assert( pQuery != nullptr );
        auto xTup = pQuery->get( );
        SvCall xRet( std::get<1>( xTup ), // uiFromStart
                     std::get<2>( xTup ), // uiToStart
                     std::get<3>( xTup ), // uiFromSize
                     std::get<4>( xTup ), // uiToSize
                     std::get<5>( xTup ), // from_forward
                     std::get<6>( xTup ), // to_forward
                     std::get<8>( xTup ), // supporting_reads
                     std::get<10>( xTup ) // supporting_nt
        );
        xRet.uiReferenceAmbiguity = std::get<9>( xTup );
        xRet.pInsertedSequence = std::get<7>( xTup ).pNucSeq;
        xRet.iId = std::get<0>( xTup );

        if( bWithSupport )
        {
            xQuerySupport.execAndFetch( std::get<0>( xTup ) );
            while( !xQuerySupport.eof( ) )
            {
                auto xTup = xQuerySupport.get( );
                xRet.vSupportingJumpIds.push_back( std::get<8>( xTup ) );
                xRet.vSupportingJumps.push_back( std::make_shared<SvJump>(
                    std::get<0>( xTup ), std::get<1>( xTup ), std::get<2>( xTup ), std::get<3>( xTup ),
                    std::get<4>( xTup ), std::get<5>( xTup ), std::get<6>( xTup ), std::get<7>( xTup ),
                    std::get<8>( xTup ), std::get<9>( xTup ) ) );
                xQuerySupport.next( );
            } // while
        } // if

        pQuery->next( );
        return xRet;
    } // method

    /**
     * @brief returns true if there is another call to fetch using libMSV::SvCallsFromDb::next
     */
    bool hasNext( )
    {
        assert( pQuery != nullptr );
        return !pQuery->eof( );
    } // method

    // pair(list(tuple(x, calls with score > x, true positives with score > x)), |gt|)
    std::tuple<std::vector<std::tuple<double, uint32_t, uint32_t>>, std::vector<std::pair<uint32_t, uint32_t>>,
               uint32_t>
    count( int64_t iSvCallerIdA, int64_t iSvCallerIdB, int64_t iAllowedDist, int64_t iAllowedDistMin,
           int64_t iAllowedDistMax, int64_t iAllowedDistStep, double dMinScore, double dMaxScore, double dStep )
    {
        std::vector<std::tuple<double, uint32_t, uint32_t>> vRet;
        std::bitset<ConfigFlags::DUMMY_FOR_COUNT> xConfig;
        xConfig.set( ConfigFlags::Overlapping );
        xConfig.set( ConfigFlags::WithIntersection );
        xConfig.set( ConfigFlags::WithSelfIntersection );
        xConfig.set( ConfigFlags::WithMinScore );
        xConfig.set( ConfigFlags::WithMaxScore );
        xConfig.set( ConfigFlags::JustCount );
        initQuery( xConfig );
        for( double dCurr = dMinScore; dCurr < dMaxScore; dCurr += dStep )
        {
            vRet.emplace_back( dCurr,
                               pSvCallTable->numCalls( iSvCallerIdA, dCurr ),
                               pQueryCount->scalar( iSvCallerIdA, dCurr, dMaxScore, iSvCallerIdB, iAllowedDist,
                                                    iAllowedDist, iAllowedDist, iAllowedDist ) );
        } // for
        std::vector<std::pair<uint32_t, uint32_t>> vRet2;
        for( int64_t iAllowedDist = iAllowedDistMin; iAllowedDist < iAllowedDistMax; iAllowedDist += iAllowedDistStep )
        {
            vRet2.emplace_back( iAllowedDist,
                                pQueryCount->scalar( iSvCallerIdA, dMinScore, dMaxScore, iSvCallerIdB, iAllowedDist,
                                                     iAllowedDist, iAllowedDist, iAllowedDist ) );
        } // for
        return std::make_tuple( vRet, vRet2, pSvCallTable->numCalls( iSvCallerIdB, 0 ) );
    } // method

    std::tuple<std::vector<std::pair<double, uint32_t>>, std::vector<std::pair<double, uint32_t>>,
               std::vector<std::pair<double, uint32_t>>, double>
    count_by_supp_nt( int64_t iSvCallerIdA, int64_t iSvCallerIdB, int64_t iAllowedDist, size_t uiNumSteps,
                      double dMinScore, double dMaxScore, double dMaxAvgSupp )
    {
        double dStep = dMaxAvgSupp / uiNumSteps;

        std::vector<std::pair<double, uint32_t>> vTruePositives;
        std::bitset<ConfigFlags::DUMMY_FOR_COUNT> xConfig;
        xConfig.set( ConfigFlags::Overlapping );
        xConfig.set( ConfigFlags::WithIntersection );
        xConfig.set( ConfigFlags::WithSelfIntersection );
        xConfig.set( ConfigFlags::WithMinScore );
        xConfig.set( ConfigFlags::WithMaxScore );
        xConfig.set( ConfigFlags::JustCount );
        xConfig.set( ConfigFlags::WithAvgSuppNtRange );
        initQuery( xConfig );
        for( double dStart = 0; dStart < dMaxAvgSupp; dStart += dStep )
        {
            double dEnd = dStart + dStep;
            vTruePositives.emplace_back( dStart + dStep / 2,
                                         pQueryCount->scalar( iSvCallerIdA, dMinScore, dMaxScore, dStart, dEnd,
                                                              iSvCallerIdB, iAllowedDist, iAllowedDist, iAllowedDist,
                                                              iAllowedDist ) );
        } // for
        std::vector<std::pair<double, uint32_t>> vFalsePositives;
        xConfig.reset( );
        xConfig.set( ConfigFlags::WithIntersection );
        xConfig.set( ConfigFlags::WithSelfIntersection );
        xConfig.set( ConfigFlags::WithMinScore );
        xConfig.set( ConfigFlags::WithMaxScore );
        xConfig.set( ConfigFlags::JustCount );
        xConfig.set( ConfigFlags::WithAvgSuppNtRange );
        initQuery( xConfig );
        for( double dStart = 0; dStart < dMaxAvgSupp; dStart += dStep )
        {
            double dEnd = dStart + dStep;
            vFalsePositives.emplace_back( dStart + dStep / 2,
                                          pQueryCount->scalar( iSvCallerIdA, dMinScore, dMaxScore, dStart, dEnd,
                                                               iSvCallerIdB, iAllowedDist, iAllowedDist, iAllowedDist,
                                                               iAllowedDist ) );
        } // for
        std::vector<std::pair<double, uint32_t>> vFalseNegatives;
        xConfig.reset( );
        xConfig.set( ConfigFlags::WithIntersection );
        xConfig.set( ConfigFlags::WithOtherIntersection );
        xConfig.set( ConfigFlags::WithMinScoreGT );
        xConfig.set( ConfigFlags::WithMaxScoreGT );
        xConfig.set( ConfigFlags::JustCount );
        xConfig.set( ConfigFlags::WithAvgSuppNtRange );
        initQuery( xConfig );
        for( double dStart = 0; dStart < dMaxAvgSupp; dStart += dStep )
        {
            double dEnd = dStart + dStep;
            vFalseNegatives.emplace_back( dStart + dStep / 2,
                                          pQueryCount->scalar( iSvCallerIdB, dStart, dEnd, iSvCallerIdA, dMinScore,
                                                               dMaxScore, iAllowedDist, iAllowedDist, iAllowedDist,
                                                               iAllowedDist ) );
        } // for

        return std::make_tuple( vTruePositives, vFalsePositives, vFalseNegatives, dStep );
    } // method
}; // namespace libMSV

} // namespace libMSV


#ifdef WITH_PYTHON
/**
 * @brief used to expose libMSV::SvCallsFromDb to python
 */
void exportCallsFromDb( libMS::SubmoduleOrganizer& xOrganizer );
#endif