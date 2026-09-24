##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of the study on the nested and the recursive Lagrangian dual    #
#   (ldld)                                                                   #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# The study is confined to this directory: its driver ldld_bench is built
# from its own source alone, i.e., it needs neither ../tssb_solver.cpp nor
# the common_utils of tools/, and a make here builds nothing outside it but
# the objects of the SMS++ modules it links.

# basic directory
DIR = .

# this directory is one level deeper than a normal tools/<module>/, so it
# cannot include ../../makefile_common (whose relative paths assume that
# depth): the compiler and its switches are here instead

CC = clang++

COMMON_SW = -std=c++20 -ferror-limit=1 -Wno-deprecated-declarations \
	-Wno-enum-compare -Wno-backslash-newline-escape
SW = -O3 $(COMMON_SW) -DNDEBUG

.DEFAULT_GOAL := all

# define & include the necessary modules- - - - - - - - - - - - - - - - - - -
# each module outputs some macros to be used here:
# *OBJ is the final object(s) / library
# *LIB is the external libraries + -L< libdirs >
# *H   is the list of all include files
# *INC is the -I< include directories >

# SMS++
SMS++SDR = ../../../SMS++
include $(SMS++SDR)/lib/makefile-c

# MultiStageStochasticBlock (and TwoStageStochasticBlock with it)
MSSBkSDR = ../../../MultiStageStochasticBlock
include $(MSSBkSDR)/makefile-s

# UCBlock
UCBckDIR = ../../../UCBlock
include $(UCBckDIR)/lib/makefile

# BundleSolver (and MILPSolver with it)
BNDSLVSDR = ../../../BundleSolver
include $(BNDSLVSDR)/makefile-s

# LagrangianDualSolver
LgDSLVSDR = ../../../LagrangianDualSolver
include $(LgDSLVSDR)/makefile

# CapacitatedFacilityLocationBlock (and BinaryKnapsackBlock with it)
CFLBkSDR = ../../../CapacitatedFacilityLocationBlock
include $(CFLBkSDR)/makefile-s

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(SMS++OBJ) $(MSSBkOBJ) $(BNDSLVOBJ) $(LgDSLVOBJ) $(UCBckOBJ) \
	$(CFLBkOBJ)

# libraries
MLIB =  $(SMS++LIB) $(MSSBkLIB) $(BNDSLVLIB) $(LgDSLVLIB) $(UCBckLIB) \
	$(CFLBkLIB)

all: $(DIR)/ldld_bench $(DIR)/cfl_tssb_gen

$(DIR)/ldld_bench: $(DIR)/ldld_bench.o $(MOBJ)
	$(CC) -o $(DIR)/ldld_bench $^ $(MLIB) $(SW)

# the generator of the stochastic facility location instances
$(DIR)/cfl_tssb_gen: $(DIR)/cfl_tssb_gen.o $(MOBJ)
	$(CC) -o $(DIR)/cfl_tssb_gen $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(MSSBkINC) $(BNDSLVINC) $(LgDSLVINC) $(UCBckINC) \
	$(CFLBkINC)

# includes
MH =    $(SMS++H) $(MSSBkH) $(BNDSLVH) $(LgDSLVH) $(UCBckH) \
	$(CFLBkH)

# compile command

$(DIR)/ldld_bench.o: $(DIR)/ldld_bench.cpp $(MH)
	$(CC) -c $(DIR)/ldld_bench.cpp -o $@ $(MINC) $(SW)

$(DIR)/cfl_tssb_gen.o: $(DIR)/gen/cfl_tssb_gen.cpp $(MH)
	$(CC) -c $(DIR)/gen/cfl_tssb_gen.cpp -o $@ $(MINC) $(SW)

# clean target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

clean::
	rm -f $(DIR)/*.o $(DIR)/*~ $(DIR)/ldld_bench $(DIR)/cfl_tssb_gen

.PHONY: all clean

############################ End of makefile #################################
