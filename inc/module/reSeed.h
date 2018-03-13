/** 
 * @file reSeed.h
 * @brief Implements a segmentation algorithm.
 * @author Markus Schmidt
 */

#ifndef RE_SEED_H
#define RE_SEED_H

//#define DEBUG_ENABLED

#include "util/system.h"
#include "module/module.h"
#include "container/segment.h"
#include "util/threadPool.h"


namespace libMA
{
    /**
     * @brief Computes a set of maximal non-enclosed seeds.
     * @ingroup module
     */
    class ReSeed : public Module{
    private:

        /*
        * bwa style extension
        */
        void extend(
                std::shared_ptr<SegmentVector> pxVector,
                nucSeqIndex extend,
                std::shared_ptr<FMIndex> pFM_index,
                std::shared_ptr<NucSeq> pQuerySeq
            );


    public:
        t_bwtIndex maxAmbiguity = 10;

        ReSeed() {}//default constructor
        ReSeed(t_bwtIndex maxAmbiguity) 
            :
            maxAmbiguity(maxAmbiguity)
        {}///constructor

        
        std::shared_ptr<Container> EXPORTED execute(std::shared_ptr<ContainerVector> vpInput);

        /**
         * @brief Used to check the input of execute.
         * @details
         * Returns:
         * - FMIndex
         * - SegmentVector
         * - NucSeq
         */
        ContainerVector EXPORTED getInputType() const;

        /**
         * @brief Used to check the output of execute.
         * @details
         * Returns:
         * - SegmentVector
         */
        std::shared_ptr<Container> EXPORTED getOutputType() const;
    };//class
}//namespace libMA


/**
 * @brief exports the Segmentation @ref Module "module" to python.
 * @ingroup export
 */
void exportReSeed();

#endif