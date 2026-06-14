##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of the LD-iteration timing study (lagrangian_timing)            #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# This study is confined here so the base ucblock_solver never carries the
# experimental SAVE_TUB per-iteration ThermalUnitBlock dump: a normal `make`
# of ../ leaves SAVE_TUB at 0. Here the SAME ../ucblock_solver.cpp is
# recompiled WITH -DSAVE_TUB=1, plus the timing_harness benchmark, in-place.
#
# Knob: PARMIN overrides the ThermalUnitDPSolver parallel horizon threshold
# (TUDPS_PAR_MIN_N); unset = the in-code default. `make PARMIN=0` forces the
# parallel DP on every horizon. NB: PARMIN recompiles the SHARED UCBlock
# object ThermalUnitDPSolver.o; rebuild ../ afterwards.

# basic directory
DIR = .

# common stuff
# this directory is one level deeper than a normal tools/<module>/, so it
# cannot include ../makefile_common (whose relative paths assume that depth);
# the compiler, the switches and the common_utils rule are inlined instead.

CC = clang++

COMMON_SW = -std=c++20 -ferror-limit=1 -Wno-deprecated-declarations -Wno-enum-compare -Wno-backslash-newline-escape -DCLANG_1200_0_32_27_PATCH
SW_RELEASE = -O3 $(COMMON_SW) -DNDEBUG

# study switches: SAVE_TUB is always on here; PARMIN (if given) overrides the
# parallel-DP horizon threshold
SAVE_TUB_SW = -DSAVE_TUB=1
ifdef PARMIN
  PARMIN_SW = -DTUDPS_PAR_MIN_N=$(PARMIN)
endif

SW = $(SW_RELEASE) $(SAVE_TUB_SW) $(PARMIN_SW)

.DEFAULT_GOAL := all

# define & include the necessary modules- - - - - - - - - - - - - - - - - - -
# if a module is not used in the current configuration, just comment out the
# corresponding include line
# each module outputs some macros to be used here:
# *OBJ is the final object(s) / library
# *LIB is the external libraries + -L< libdirs >
# *H   is the list of all include files
# *INC is the -I< include directories >

# define input macros for SMS++ complete makefile, then include it
SMS++SDR = ../../../SMS++
include $(SMS++SDR)/lib/makefile-c

# UCBlock
UCBckDIR = ../../../UCBlock
include $(UCBckDIR)/lib/makefile

# BundleSolver
BNDSLVSDR = ../../../BundleSolver
include $(BNDSLVSDR)/makefile-s

# LagrangianDualSolver
LgDSLVSDR = ../../../LagrangianDualSolver
include $(LgDSLVSDR)/makefile-s

# common_utils (in tools/, two levels up from here)
CMMNUOBJ = ../../common_utils.o
CMMNUH = ../../common_utils.h
CMMNUINC = -I../..

$(CMMNUOBJ): $(CMMNUH) $(SMS++H) ../../common_utils.cpp
	$(CC) -c ../../common_utils.cpp -o $@ $(CMMNUINC) $(SMS++INC) $(SW)

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(SMS++OBJ) $(BNDSLVOBJ) $(LgDSLVOBJ) $(UCBckOBJ) $(CMMNUOBJ)

# libraries
MLIB =  $(SMS++LIB) $(BNDSLVLIB) $(LgDSLVLIB) $(UCBckLIB)

all: $(DIR)/ucblock_solver $(DIR)/timing_harness

# the SAVE_TUB-enabled solver, built from the base source into THIS directory
# (its object is local: it never clobbers the base ../ucblock_solver.o)
$(DIR)/ucblock_solver: $(DIR)/ucblock_solver.o $(MOBJ)
	$(CC) -o $(DIR)/ucblock_solver $^ $(MLIB) $(SW)

# timing_harness: per-dump benchmark of stdDP / parDP / extDP / :MILPSolver
$(DIR)/timing_harness: $(DIR)/timing_harness.o $(MOBJ)
	$(CC) -o $(DIR)/timing_harness $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(BNDSLVINC) $(LgDSLVINC) $(UCBckINC) $(CMMNUINC)

# includes
MH =    $(SMS++H) $(BNDSLVH) $(LgDSLVH) $(UCBckH) $(CMMNUH)

# compile command

$(DIR)/ucblock_solver.o: ../ucblock_solver.cpp $(MH)
	$(CC) -c ../ucblock_solver.cpp -o $@ $(MINC) $(SW)

$(DIR)/timing_harness.o: $(DIR)/timing_harness.cpp $(MH)
	$(CC) -c $(DIR)/timing_harness.cpp -o $@ $(MINC) $(SW)

# clean target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

clean::
	rm -f $(DIR)/*.o $(DIR)/*~ $(DIR)/ucblock_solver $(DIR)/timing_harness

.PHONY: all clean

############################ End of makefile #################################
