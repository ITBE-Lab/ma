/**
 * @file binarySeeding.h
 * @brief Implements a segmentation algorithm.
 * @author Markus Schmidt
 */
#ifndef OTHER_SEEDING_H
#define OTHER_SEEDING_H

#include "ma/container/segment.h"
#include "ms/module/module.h"
#include "util/system.h"
#include "util/threadPool.h"

namespace libMA
{
class PerfectMatch;

/**
 * @brief Computes a maximally covering set of seeds.
 * @details
 * Can use either the extension scheme by Li et Al. or ours.
 * @ingroup module
 */
class OtherSeeding : public libMS::Module<SegmentVector, false, FMIndex, NucSeq>
{
  public:
    const bool bBowtie;

    void bowtieExtension( std::shared_ptr<FMIndex> pFM_index, std::shared_ptr<NucSeq> pQuerySeq,
                          std::shared_ptr<SegmentVector> pSegmentVector );

    void doBlasrExtension( std::shared_ptr<FMIndex> pFM_index, std::shared_ptr<NucSeq> pQuerySeq,
                           std::shared_ptr<SegmentVector> pSegmentVector );

  public:
    /**
     * @brief Initialize a OtherSeeding Module
     */
    OtherSeeding( const ParameterSetManager& rParameters, bool bBowtie ) : bBowtie( bBowtie )
    {} // constructor

    // overload
    virtual std::shared_ptr<SegmentVector> DLL_PORT(MA) execute( std::shared_ptr<FMIndex> pFM_index,
                                                             std::shared_ptr<NucSeq> pQuerySeq );

}; // class

} // namespace libMA


#ifdef WITH_PYTHON
/**
 * @brief exports the Segmentation @ref libMA::Module "module" to python.
 * @ingroup export
 */
#ifdef WITH_BOOST
void exportOtherSeeding( );
#else
void exportOtherSeeding( libMS::SubmoduleOrganizer& xOrganizer );
#endif
#endif

#endif