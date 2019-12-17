from bokeh.plotting import figure
from bokeh.models.tools import HoverTool
from bokeh.plotting import ColumnDataSource
from bokeh.events import Tap
import math
import copy

class SeedPlot:
    def __init__(self, main_plot, renderer):
        self.left_plot = figure(
            width=300,
            height=900,
            y_range=main_plot.plot.y_range,
            tools=["xpan", "xwheel_zoom"],
            active_scroll="xwheel_zoom",
            toolbar_location=None
        )
        self.left_plot.xaxis.major_label_orientation = math.pi/2
        self.left_plot.yaxis.axis_label = "Reference Position"
        self.left_plot.xaxis.axis_label = "Read Id"

        self.bottom_plot = figure(
            width=900,
            height=300,
            x_range=main_plot.plot.x_range,
            y_range=self.left_plot.x_range,
            tools=["ypan", "ywheel_zoom"],
            active_scroll="ywheel_zoom",
            toolbar_location=None
        )
        self.bottom_plot.xaxis.axis_label = "Reference Position"
        self.bottom_plot.yaxis.axis_label = "Read Id"

        # ambigious regions (red rectangles)
        self.ambiguous_regions = ColumnDataSource({"l":[], "b":[], "r":[], "t":[]})
        self.bottom_plot.quad(left="l", bottom="b", right="r", top="t", fill_alpha=0.5,
                            fill_color="red", line_width=0, source=self.ambiguous_regions, name="ambiguous_regions")
        self.left_plot.quad(left="b", bottom="l", right="t", top="r", fill_alpha=0.5,
                            fill_color="red", line_width=0, source=self.ambiguous_regions, name="ambiguous_regions")
                            
        hover_ambiguous_regions = HoverTool(tooltips=[("left", "@l"),
                                                      ("bottom", "@b"),
                                                      ("right", "@r"),
                                                      ("top", "@t"),
                                                      ("fill percentage", "@f"),
                                                      ("additional seed size", "@s")],
                                            names=['ambiguous_regions'],
                                            name="Hover ambiguous regions")
        self.left_plot.add_tools(hover_ambiguous_regions)
        self.bottom_plot.add_tools(hover_ambiguous_regions)

        # seeds
        self.seeds = ColumnDataSource({"category":[], "center":[], "size":[], "c":[]})
        self.left_plot.rect(x="category", y="center", width=1, height="size",
                            fill_color="c", line_width=0, source=self.seeds, name="seeds")
        self.bottom_plot.rect(y="category", x="center", height=1, width="size",
                              fill_color="c", line_width=0, source=self.seeds, name="seeds")

        hover_seeds = HoverTool(tooltips=[("read id", "@r_id"),
                                          ("q, r, l", "@q, @r, @l"),
                                          ("index", "@idx"),
                                          ("reseeding-layer", "@layer"),
                                          ("parlindrome-filtered", "@parlindrome")],
                                names=['seeds'],
                                name="Hover reads")
        self.left_plot.add_tools(hover_seeds)
        self.bottom_plot.add_tools(hover_seeds)

        # make seeds clickable
        self.bottom_plot.on_event(Tap, lambda tap: self.seed_tap(renderer, tap.x, tap.y))
        self.left_plot.on_event(Tap, lambda tap: self.seed_tap(renderer, tap.y, tap.x))

    def update_selection(self, renderer):
        def highlight_seed(condition):
            if len(self.seeds.data["c"]) > 0:
                repl_dict = copy.copy(self.seeds.data)
                max_seed_size = max(repl_dict["size"])
                for idx, _ in enumerate(repl_dict["c"]):
                    if condition(idx):
                        if repl_dict["layer"][idx] == -1:
                            repl_dict["c"][idx] = Plasma256[ (255 * repl_dict["size"][idx]) // max_seed_size]
                        elif repl_dict["parlindrome"][idx]:
                            repl_dict["c"][idx] = "red"
                        elif repl_dict["f"][idx]: # on forward strand
                            repl_dict["c"][idx] = "green"
                        else:
                            repl_dict["c"][idx] = "purple"
                    else:
                        repl_dict["c"][idx] = "lightgrey"
                # this copy is inefficient
                self.seeds.data = repl_dict

        if not renderer.selected_seed_id is None:
            highlight_seed(lambda idx: (self.seeds.data["idx"][idx],
                                        self.seeds.data["r_id"][idx]) == renderer.selected_seed_id)
        elif not renderer.selected_read_id is None:
            highlight_seed(lambda idx: self.seeds.data["r_id"][idx] == renderer.selected_read_id)
        else:
            highlight_seed(lambda idx: True)

    def seed_tap(self, renderer, x, y):
        renderer.selected_seed_id = None
        renderer.selected_read_id = None
        for idx, _ in enumerate(self.seeds.data["center"]):
            if self.seeds.data["category"][idx] - 1/2 <= y and \
               self.seeds.data["category"][idx] + 1/2 >= y:
                if self.seeds.data["center"][idx] - self.seeds.data["size"][idx]/2 <= x and \
                   self.seeds.data["center"][idx] + self.seeds.data["size"][idx]/2 >= x:
                    renderer.selected_seed_id = (self.seeds.data["idx"][idx], self.seeds.data["r_id"][idx])
                    renderer.selected_read_id = self.seeds.data["r_id"][idx]
                    break
                if not renderer.do_compressed_seeds:
                    renderer.selected_read_id = self.seeds.data["r_id"][idx]
        self.update_selection(renderer)
        renderer.read_plot.nuc_plot.copy_nts(renderer)
        renderer.read_plot.copy_seeds(renderer, lambda idx: self.seeds.data["r_id"][idx] == renderer.selected_read_id)
        renderer.main_plot.update_selection(renderer)