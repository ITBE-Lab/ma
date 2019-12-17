from MA import AnalyzeRuntimes, ParameterSetManager
import datetime
from .visual_elements.main_plot import *
from .visual_elements.seed_plot import *
from .visual_elements.nuc_plot import *
from .visual_elements.read_plot import *
from .visual_elements.widgets import *

class Renderer():
    def __init__(self):
        self.main_plot = MainPlot(self)
        self.nuc_plot = NucPlot(self.main_plot)
        self.seed_plot = SeedPlot(self.main_plot)
        self.read_plot = ReadPlot(self.nuc_plot, self)
        self.widgets = Widgets()
        self.pack = None
        self.fm_index = None
        self.sv_db = None
        self.w = None
        self.h = None
        self.params = ParameterSetManager()
        self.quads = []
        self.read_ids = set() # @todo make two dicts? read ids new and read ids old?
        self.give_up_factor = 1000
        self.analyze = AnalyzeRuntimes()
        self.do_render_seeds = True
        self.do_compressed_seeds = True
        self.render_mems = False
        self.xs = 0
        self.ys = 0
        self.xe = 0
        self.ye = 0
        self.redered_everything = False
        self.render_area_factor = 1 # @todo make this adjustable 
        self.selected_read_id = -1
    
    def get_run_id(self):
        return int(self.widgets.run_id_dropdown.value)

    def get_gt_id(self):
        return int(self.widgets.ground_truth_id_dropdown.value)

    def get_min_score(self):
        return self.widgets.score_slider.value

    def get_max_num_ele(self):
        return self.widgets.max_elements_slider.value

    def measure(self, name):
        class MeasureHelper:
            def __init__(self, name, analyze):
                self.name = name
                self.start = None
                self.analyze = analyze
            def __enter__(self):
                self.start = datetime.datetime.now()
            def __exit__(self, exc_type, exc_val, exc_tb):
                end = datetime.datetime.now()
                delta = end - self.start
                self.analyze.register(name, delta.total_seconds(), lambda x: x)
        return MeasureHelper(name, self.analyze)


    # imported methdos
    from ._render import render
    from ._setup import setup
    from ._render_overview import render_overview
    from ._render_calls import render_calls
    from ._render_jumps import render_jumps
    from ._render_reads import add_seed
    from ._render_reads import add_rectangle
    from ._render_reads import render_reads
    from ._render_nucs import render_nucs
