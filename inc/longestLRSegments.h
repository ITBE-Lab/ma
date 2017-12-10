/** 
 * @file longestNonEnclosedSegments.h
 * @brief Implements a segmentation algorithm.
 * @author Markus Schmidt
 */
#ifndef LONGEST_L_R_SEGMENTS_H
#define LONGEST_L_R_SEGMENTS_H

#define DEBUG_ENABLED

#include "system.h"
#include "cppModule.h"
#include "segmentList.h"
#include "threadPool.h"


class PerfectMatch;

/**
 * @brief Computes a set of non-enclosed seeds.
 * @details
 * This is the seeding algorithm presented in our paper.
 * Gives you vew good quality seeds.
 * @ingroup module
 */
class LongestLRSegments : public CppModule{
private:

	static SA_IndexInterval extend_backward(
			const SA_IndexInterval &ik, 
			const uint8_t c, 
			std::shared_ptr<FM_Index> pFM_index
		);

	/* perform forward or backwards extension (depending on bBackwards) int the given interval pxNode
	*  starts at uiStartIndex and will save any matches longer than uiMinIntervalSize in pxNode if the
	*  current extension could reach further than uiOnlyRecordHitsFurtherThan
	*/
	static Segment extend(
			std::shared_ptr<SegmentListInterval> pxNode,
			std::shared_ptr<FM_Index> pFM_index,
			std::shared_ptr<NucleotideSequence> pQuerySeq
		);
	/*
	*	does nothing if the given interval can be found entirely on the genome.
	*	if the interval cannot be found this method splits the interval in half and repeats the step with the first half,
	*	while queuing the second half as a task in the thread pool.
	*/
	static void procesInterval(
			size_t uiThreadId, 
			SegmentList::iterator pxNode, 
			std::shared_ptr<SegmentList> pSegmentList,
			std::shared_ptr<FM_Index> pFM_index,
			std::shared_ptr<NucleotideSequence> pQuerySeq,
			ThreadPoolAllowingRecursiveEnqueues* pxPool
		);


public:
	
	std::shared_ptr<Container> execute(ContainerVector vpInput);

	ContainerVector getInputType() const;
    std::shared_ptr<Container> getOutputType() const;

    std::string getName() const
    {
        return "LongestLRSegments";
    }
};//class


/**
 * @brief exports the Segmentation @ref CppModule "module" to python.
 * @ingroup export
 */
void exportLongestLRSegments();

#endif