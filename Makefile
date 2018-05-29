##
# switches:
# DEBUG: enables sanity checks within the code
# WITH_PYTHON: compiles libMA in so that it can be imported in python; requires boost & python
# WITH_GPU_SW: compiles a gpu implementation of the SW algorithm; requires libCuda
#

# location of the Boost Python include files and library
# $(BOOST_ROOT) must be set in the system environment!
BOOST_LIB_PATH = $(BOOST_ROOT)/stage/lib/
BOOST_LIB = boost_python3

LIBGABA_HOME = ./libGaba
 
# target files
TARGET = $(subst .cpp,,$(subst src/,,$(wildcard src/*/*.cpp)))

TARGET_OBJ= \
	$(addprefix obj/,$(addsuffix .o,$(TARGET))) \
	obj/ksw/ksw2_dispatch.co \
	obj/ksw/ksw2_extz2_sse2.co \
	obj/ksw/ksw2_extz2_sse41.co \
	obj/container/qSufSort.co

# flags
CC=gcc
CCFLAGS=-Wall -Werror -fPIC -std=c++11 -mavx2 -O3
DEBUG_CCFLAGS=-Wall -Werror -fPIC -std=c++11 -mavx2 -g -DDEBUG_LEVEL=1
CFLAGS=-Wall -Werror -fPIC -O3
LDFLAGS= -std=c++11 -shared -Wl,--export-dynamic
LDLIBS= -L$(LIBGABA_HOME) -lm -lpthread -lstdc++ -lgaba
INCLUDES= -isystem$(LIBGABA_HOME)/ -Iinc

# this adds debug switches
ifeq ($(DEBUG), 1)
	# we store release and debug objects in different folders
	# no debug version for the ksw library
	TARGET_OBJ= \
		$(addprefix dbg/,$(addsuffix .o,$(TARGET))) \
		obj/ksw/ksw2_dispatch.co \
		obj/ksw/ksw2_extz2_sse2.co \
		obj/ksw/ksw2_extz2_sse41.co \
		obj/container/qSufSort.co
endif

# add configuration for python
ifeq ($(WITH_PYTHON), 1)
	CCFLAGS += -DWITH_PYTHON -DBOOST_ALL_DYN_LINK
	DEBUG_CCFLAGS += -DWITH_PYTHON -DBOOST_ALL_DYN_LINK 
	CFLAGS += -DWITH_PYTHON
	LDLIBS += $(PYTHON_LIB) -L$(BOOST_LIB_PATH)
	LDLIBS += $(addprefix -l,$(addsuffix $(BOOST_SUFFIX),$(BOOST_LIB)))
	INCLUDES += -isystem$(PYTHON_INCLUDE)/ -isystem$(BOOST_ROOT)/
endif

# compile the gpu smith waterman as well
ifeq ($(WITH_GPU_SW), 1)
	TARGET_OBJ += sw_gpu.o
	CCFLAGS += -DWITH_GPU_SW
	LDLIBS += -L/usr/local/cuda-9.1/lib64/ -lcudart
endif

# primary target
all: dirs ma

# create build directories
dirs:
	mkdir obj obj/module obj/container obj/util obj/ksw \
		  dbg dbg/module dbg/container dbg/util dbg/ksw

# executable target
ma: libMA src/cmdMa.cpp
	$(CC) $(CCFLAGS) src/cmdMa.cpp $(INCLUDES) $(LDLIBS) libMA.so -o $@

# library target
libMA: $(TARGET_OBJ) $(LIBGABA_HOME)/libgaba.a
	$(CC) $(LDFLAGS) $(TARGET_OBJ) $(LDLIBS) -o libMA.so

# special targets for the ksw2 library
obj/ksw/ksw2_dispatch.co:src/ksw/ksw2_dispatch.c inc/ksw/ksw2.h
	$(CC) -c $(CFLAGS) -Iinc -DKSW_CPU_DISPATCH $< -o $@

obj/ksw/ksw2_extz2_sse2.co:src/ksw/ksw2_extz2_sse.c inc/ksw/ksw2.h
	$(CC) -c $(CFLAGS) -Iinc -msse2 -mno-sse4.1 -DKSW_CPU_DISPATCH -DKSW_SSE2_ONLY $< -o $@

obj/ksw/ksw2_extz2_sse41.co:src/ksw/ksw2_extz2_sse.c inc/ksw/ksw2.h
	$(CC) -c $(CFLAGS) -Iinc -msse4.1 -DKSW_CPU_DISPATCH $< -o $@

obj/container/qSufSort.co:src/container/qSufSort.c inc/container/qSufSort.h
	$(CC) -c $(CFLAGS) -Iinc $< -o $@

# target for debug object files
dbg/%.o: src/%.cpp inc/%.h
	$(CC) $(DEBUG_CCFLAGS) $(INCLUDES) -c $< -o $@

# target for object files
obj/%.o: src/%.cpp inc/%.h
	$(CC) $(CCFLAGS) $(INCLUDES) -c $< -o $@

# the gaba library
libGaba/libgaba.a: 
	$(MAKE) -C $(LIBGABA_HOME)

# the gpu smith waterman
sw_gpu.o:
	$(MAKE) -f CUDA_Makefile

# documentation generation
html/index.html: $(wildcard inc/*) $(wildcard inc/*/*) $(wildcard src/*) $(wildcard src/*/*) $(wildcard MA/*.py) doxygen.config
	doxygen doxygen.config

# currently disabled
#install: all
#	pip3 install . --upgrade --no-cache-dir #no pip installation at the moment
#	cp libMA.so /usr/lib
#	pip3 show MA #no pip installation at the moment
#distrib:
#	python setup.py sdist bdist_egg bdist_wheel

# @todo remove me
vid:
	gource -f --seconds-per-day 0.1

clean:
	rm -f -r obj dbg libMA.so
	rm -r -f html
	$(MAKE) -C $(LIBGABA_HOME) clean
	$(MAKE) -f CUDA_Makefile clean
#	rm -r -f dist *.egg-info build

docs: html/index.html

.Phony: all clean docs vid libMA # install distrib
