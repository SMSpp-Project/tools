##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of the study on the re-optimization of the Min-Cost Flow       #
#   problem (reopt)                                                          #
#                                                                            #
#                               Donato Meoli                                 #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# The study is confined to this directory: its driver reopt_bench is built
# from its own source alone, i.e., it needs neither ../mcfblock_solver.cpp
# nor the common_utils of tools/, and a make here builds nothing outside it
# but the objects of the SMS++ modules it links.

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

# MCFClassSolver (and MCFBlock and SMS++ with it)
MCFCSSDR = ../../../MCFClassSolver
include $(MCFCSSDR)/makefile-s

# MCFLemonSolver, whose MCFBlock and SMS++ are already there
MCFLESDR = ../../../MCFLemonSolver
include $(MCFLESDR)/makefile

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(MCFCSOBJ) $(MCFLEOBJ)

# libraries
MLIB =  $(MCFCSLIB) $(MCFLELIB)

all: $(DIR)/reopt_bench

$(DIR)/reopt_bench: $(DIR)/reopt_bench.o $(MOBJ)
	$(CC) -o $(DIR)/reopt_bench $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(MCFCSINC) $(MCFLEINC)

# includes
MH =    $(MCFCSH) $(MCFLEH)

# compile command

$(DIR)/reopt_bench.o: $(DIR)/reopt_bench.cpp $(MH)
	$(CC) -c $(DIR)/reopt_bench.cpp -o $@ $(MINC) $(SW)

# clean target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

clean::
	rm -f $(DIR)/*.o $(DIR)/*~ $(DIR)/reopt_bench

.PHONY: all clean

############################ End of makefile #################################
