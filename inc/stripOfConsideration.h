/** 
 * @file bucketing.h
 * @brief Implements a Bucketing.
 * @author Markus Schmidt
 */
#ifndef STRIP_OF_CONSIDERATION_H
#define STRIP_OF_CONSIDERATION_H

#include "segment.h"
#include "module.h"

namespace libLAuS
{
	/**
	 * @brief Used to quickly find areas with high density of @ref Seed "seeds".
	 * @ingroup module
	 */
	class StripOfConsideration: public Module
	{
	private:

	public:
		/// @brief The strip of consideration size.
		nucSeqIndex uiStripSize = 10000;
		/// @brief Maximum ambiguity for a seed to be considered.
		unsigned int uiMaxHitsPerInterval = 500;
		/**
		* @brief skip seeds with too much ambiguity
		* @details
		* True: skip all seeds with to much ambiguity
		* False: use max_hits instances of the seeds with more ambiguity
		*/
		bool bSkipLongBWTIntervals = true;
		
	private:
		inline nucSeqIndex getPositionForBucketing(nucSeqIndex uiQueryLength, const Seed xS) const 
		{ 
			return xS.start_ref() + (uiQueryLength - xS.start()); 
		}//function

		void forEachNonBridgingSeed(
				std::shared_ptr<SegmentVector> pVector,
				std::shared_ptr<FMIndex> pxFM_index,std::shared_ptr<Pack> pxRefSequence,
				std::shared_ptr<NucSeq> pxQuerySeq,
				std::function<void(Seed)> fDo,
				nucSeqIndex addSize// = 0 (default)
			);

	public:

		StripOfConsideration(){}//constructor

		std::shared_ptr<Container> execute(ContainerVector vpInput);

		/**
		 * @brief Used to check the input of execute.
		 * @details
		 * Returns:
		 * - SegmentVector
		 * - Seeds
		 * - NucSeq
		 * - Pack
		 * - FMIndex
		 */
		ContainerVector getInputType() const;

		/**
		 * @brief Used to check the output of execute.
		 * @details
		 * Returns:
		 * - ContainerVector(Seeds)
		 */
		std::shared_ptr<Container> getOutputType() const;

		std::string getName() const
		{
			return "StripOfConsideration";
		}
	};//class
}//namspace libLAuS

/**
 * @brief export the bucketing @ref Module "module" to python.
 * @ingroup export
 */
void exportStripOfConsideration();

#endif