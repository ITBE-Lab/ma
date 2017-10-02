from aligner import *


def set_up_aligner(query_pledge, reference_pledge, fm_index_pledge, rev_fm_index_pledge):
    seg = Segmentation(True)
    seg.bSkipLongBWTIntervals = False

    anc = NlongestIntervalsAsAnchors(2)

    bucketing = Bucketing()
    bucketing.strip_size = 50

    sweep = SweepAllReturnBest()

    nmw = NeedlemanWunsch()

    printer = AlignmentPrinter()


    segment_pledge = seg.promise_me((
        fm_index_pledge,
        rev_fm_index_pledge,
        query_pledge,
        reference_pledge
    ))

    anchors_pledge = anc.promise_me((segment_pledge,))

    strips_pledge = bucketing.promise_me((
        segment_pledge,
        anchors_pledge,
        query_pledge,
        reference_pledge,
        fm_index_pledge,
        rev_fm_index_pledge
    ))

    best_pledge = sweep.promise_me((
        strips_pledge,
        query_pledge,
        reference_pledge
    ))

    algn_pledge = nmw.promise_me((
        best_pledge,
        query_pledge,
        reference_pledge
    ))

    return printer.promise_me((algn_pledge, query_pledge, reference_pledge))