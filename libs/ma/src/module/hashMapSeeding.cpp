/**
 * @file hash_map_seeding.cpp
 * @author Markus Schmidt
 */
#include "module/hashMapSeeding.h"
#include <limits>
#include <unordered_map>

using namespace libMA;

using namespace libMA::defaults;


std::shared_ptr<Seeds> HashMapSeeding::execute( std::shared_ptr<NucSeq> pQ1, std::shared_ptr<NucSeq> pQ2 )
{
    std::unordered_multimap<std::string, size_t> xIndex;

    // insert seeds into index
    for( size_t uiI = 0; uiI < pQ2->length( ); uiI += 1 )
        xIndex.emplace( pQ2->fromTo( uiI, uiI + this->uiSeedSize ), uiI );

    auto pSeeds = std::make_shared<Seeds>( );
    for( size_t uiI = 0; uiI < pQ1->length( ); uiI += 1 )
    {
        auto tuiRange = xIndex.equal_range( pQ1->fromTo( uiI, uiI + this->uiSeedSize ) );
        for( auto xIt = tuiRange.first; xIt != tuiRange.second; ++xIt )
            pSeeds->emplace_back( uiI, this->uiSeedSize, xIt->second, true );
    } // for

    return pSeeds;
} // function

std::shared_ptr<Seeds> ReSeeding::execute( std::shared_ptr<Seeds> pSeeds, std::shared_ptr<NucSeq> pQuery,
                                           std::shared_ptr<Pack> pPack )
{

    auto pCollection = std::make_shared<Seeds>( );

    for( size_t uiI = 0; uiI < pSeeds->size( ) - 1; uiI++ )
    {
        Seed& rA = ( *pSeeds )[ uiI ];
        Seed& rB = ( *pSeeds )[ uiI + 1 ];
        assert( rA.bOnForwStrand == rB.bOnForwStrand );
        if( rA.end( ) + xHashMapSeeder.uiSeedSize <= rB.start( ) &&
            rA.end_ref( ) + xHashMapSeeder.uiSeedSize <= rB.start_ref( ) )
        {
            // reseed in the gap between seed uiI and seed uiI + 1
            auto pAppend = xHashMapSeeder.execute(
                std::make_shared<NucSeq>( rA.bOnForwStrand ? pQuery->fromTo( rA.end( ), rB.start( ) )
                                                           : pQuery->fromToComplement( rA.end( ), rB.start( ) ) ),
                pPack->vExtract( rA.end_ref( ), rB.start_ref( ) ) );
            // set on forw strand = false if necessary
            if( rA.bOnForwStrand == false )
                for( Seed& rSeed : *pAppend )
                    rSeed.bOnForwStrand = false;
            pCollection->append( pAppend );
        } // if
    } // for
    // append the original seeds
    pCollection->append( pSeeds );

    // lump everything
    return xLumper.execute( pCollection );
} // function

#ifdef WITH_PYTHON
void exportHashMapSeeding( py::module& rxPyModuleId )
{
    // export the HashMapSeeding class
    exportModule<HashMapSeeding>( rxPyModuleId, "HashMapSeeding" );
    // export the ReSeeding class
    exportModule<ReSeeding>( rxPyModuleId, "ReSeeding" );
    // export the ExtractFilledSeedSets class
    exportModule<ExtractFilledSeedSets>( rxPyModuleId, "ExtractFilledSeedSets" );
} // function
#endif
