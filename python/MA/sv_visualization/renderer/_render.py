from MA import *


def render(self):
    self.main_plot.genome_outline.data = {"x":[0], 
                                          "y":[0],
                                          "w":[self.pack.unpacked_size_single_strand],
                                          "h":[self.pack.unpacked_size_single_strand]}


    if self.xs < 0:
        self.xs = 0
    if self.ys < 0:
        self.ys = 0
    if self.xe < 0:
        self.xe = 0
    if self.ye < 0:
        self.ye = 0
    self.w = int(self.xe - self.xs)
    self.h = int(self.ye - self.ys)

    s = max(min(self.xs - self.w, self.ys - self.h), 0)
    e = min(max(self.xe + self.w, self.ye + self.h), self.pack.unpacked_size_single_strand)
    # plot diagonal; we need s and e since too large lines sometimes do not render...
    self.main_plot.diagonal_line.data = {"xs":[s, e], "ys":[s, e]}

    if self.widgets.run_id_dropdown.value is None:
        return
    if not self.sv_db.run_exists(self.get_run_id()):
        return

    with self.measure("get_call_overview_area"):
        num_ele = libMA.get_call_overview_area(self.sv_db, self.pack, self.get_run_id(),
                                               self.get_min_score(),
                                               int(self.xs - self.w), int(self.ys - self.h), self.w*3, self.h*3)
    if num_ele > self.get_max_num_ele():
        self.render_overview()
    else:
        self.render_calls()
