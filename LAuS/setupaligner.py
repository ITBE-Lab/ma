##
# @package LAuS
# @file setupaligner.py
# @brief Implements @ref LAuS.set_up_aligner "set_up_aligner".
# @author Markus Schmidt

from .__init__ import *

##
# @brief Setup the @ref comp_graph_sec "computational graph" 
# with the standard aligner comfiguration.
# @details 
# Uses following Modules in this order:
# - The given segmentation; default is LongestNonEnclosedSegments
# - NlongestIntervalsAsAnchors
# - Bucketing
# - @ref LAuS.aligner.SweepAllReturnBest "SweepAllReturnBest"
# - NeedlemanWunsch
# - @ref LAuS.alignmentPrinter.AlignmentPrinter "AlignmentPrinter"
#
# If a list of query pledges is given a list of result pledges is returned.
#
#
# Returns a list with one element for each query_pledge given.
# Each list element is a tuple of the returned pledges of every step in the alignment.
#
# @returns a list of pledge tuples
# @ingroup module
#
def set_up_aligner(query_pledges, reference_pledge, 
        fm_index_pledge, seg=LongestNonEnclosedSegments(), max_hits=500):

    anc = NlongestIntervalsAsAnchors(2)

    bucketing = Bucketing()
    bucketing.max_hits = max_hits

    sweep = SweepAllReturnBest()

    nmw = NeedlemanWunsch()

    printer = AlignmentPrinter()

    query_pledges_ = []
    if isinstance(query_pledges, list) or isinstance(query_pledges, tuple):
        query_pledges_.extend(query_pledges)
    else:
        query_pledges_.append(query_pledges)

    return_pledges = []
    return_trigger = []
    for query_pledge in query_pledges_:
        segment_pledge = seg.promise_me((
            fm_index_pledge,
            query_pledge
        ))

        anchors_pledge = anc.promise_me((segment_pledge,))


        strips_pledge = bucketing.promise_me((
            segment_pledge,
            anchors_pledge,
            query_pledge,
            reference_pledge,
            fm_index_pledge
        ))

        best_pledge = sweep.promise_me((
            strips_pledge,
            query_pledge,
            reference_pledge
        ))

        align_pledge = nmw.promise_me((
            best_pledge,
            query_pledge,
            reference_pledge
        ))

        return_pledges.append((
            segment_pledge,
            anchors_pledge,
            strips_pledge,
            best_pledge,
            align_pledge
        ))

        return_trigger.append(align_pledge)

    if isinstance(query_pledges, list) or isinstance(query_pledges, tuple):
        return return_pledges, return_trigger
    else:
        return return_pledges[0], return_trigger[0]