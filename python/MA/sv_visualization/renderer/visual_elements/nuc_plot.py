from bokeh.plotting import figure
from bokeh.models.tools import HoverTool
from bokeh.plotting import ColumnDataSource

class NucPlot:
    def __init__(self, main_plot):
        self.left_plot = figure(
            width=40,
            height=900,
            y_range=main_plot.plot.y_range,
            tools=["ypan", "ywheel_zoom"],
            active_scroll="ywheel_zoom",
            toolbar_location=None
        )
        self.left_plot.axis.visible = False
        self.left_plot.grid.visible = False

        self.bottom_plot = figure(
            width=900,
            height=40,
            x_range=main_plot.plot.x_range,
            tools=["xpan", "xwheel_zoom"],
            active_scroll="xwheel_zoom",
            toolbar_location=None
        )
        self.bottom_plot.axis.visible = False
        self.bottom_plot.grid.visible = False

        # the nucleotides
        self.left_nucs = ColumnDataSource({"p":[], "c":[]})
        self.left_plot.rect(x=0.5, y="p", width=1, height=1, fill_color="c", line_width=0,
                            source=self.left_nucs, name="nucleotides")

        self.bottom_nucs = ColumnDataSource({"p":[], "c":[]})
        self.bottom_plot.rect(x="p", y=0.5, width=1, height=1, fill_color="c", line_width=0,
                            source=self.bottom_nucs, name="nucleotides")

        hover_nucleotides = HoverTool(tooltips="@i", names=['nucleotides'], name="Hover nucleotides")
        self.left_plot.add_tools(hover_nucleotides)
        self.bottom_plot.add_tools(hover_nucleotides)