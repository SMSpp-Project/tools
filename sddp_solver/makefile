##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of sddp_solver and sddp_greedy_solver                           #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# basic directory
DIR = .

# module name
NAME = $(DIR)/sddp_solver $(DIR)/sddp_greedy_solver

# debug switches
#SW = -g3 -glldb -fno-inline -std=c++17 -ferror-limit=1 -Wno-enum-compare
# debug switches with address sanitizer and extra pedantic warning
SW = -g3 -glldb -fno-inline -std=c++17 -ferror-limit=1 -fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer -Wpedantic -Wextra -Wno-unused-parameter -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare
# production switches with address sanitizer
#SW = -O3 -std=c++17 -DNDEBUG -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -Wno-enum-compare
# production switches
#SW = -O3 -std=c++17 -DNDEBUG -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare

# compiler
CC = clang++

# default target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

default: $(DIR)/$(NAME)

# clean - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

clean::
	rm -f $(DIR)/*.o $(DIR)/*~ $(DIR)/$(NAME)

# define & include the necessary modules- - - - - - - - - - - - - - - - - - -
# if a module is not used in the current configuration, just comment out the
# corresponding include line
# each module outputs some macros to be used here:
# *OBJ is the final object(s) / library
# *LIB is the external libraries + -L< libdirs >
# *H   is the list of all include files
# *INC is the -I< include directories >

# define input macros for SMS++ complete makefile, then include it
SMS++SDR = ../../SMS++
include $(SMS++SDR)/lib/makefile-c

# BundleSolver
BNDSLVSDR = ../../BundleSolver
include $(BNDSLVSDR)/makefile-s

# LagrangianDualSolver
LgDSLVSDR = ../../LagrangianDualSolver
include $(LgDSLVSDR)/makefile

# not necessary, BundleSolver does that already
# MILPSolver
#MILPSSDR = ../../../MILPSolver
#include $(MILPSSDR)/makefile

# define input macros for SDDPBlock makefile + dependencies, then include it
SDDPBkSDR = ../../SDDPBlock
include $(SDDPBkSDR)/makefile-s

# define input macros for UCBlock makefile, then include it
UCBckDIR = ../../UCBlock
include $(UCBckDIR)/lib/makefile

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(SMS++OBJ) $(SDDPBkOBJ) $(BNDSLVOBJ) $(LgDSLVOBJ) $(UCBckOBJ) \
	$(DIR)/CutProcessing.o

# libraries
MLIB =  $(SMS++LIB) $(SDDPBkLIB) $(BNDSLVLIB) $(LgDSLVLIB) $(UCBckLIB)

$(DIR)/sddp_solver: $(DIR)/sddp_solver.o $(MOBJ)
	$(CC) -o $(DIR)/sddp_solver $^ $(MLIB) $(SW)

$(DIR)/sddp_greedy_solver: $(DIR)/sddp_greedy_solver.o $(MOBJ)
	$(CC) -o $(DIR)/sddp_greedy_solver $^ $(MLIB) $(SW)

# dependencies: every .o from its .C + every recursively included .h- - - - -

# include directives
MINC =  $(SMS++INC) $(SDDPBkINC) $(BNDSLVINC) $(LgDSLVINC) $(UCBckINC) \
	-I$(DIR) -I../ucblock_solver

# includes
MH =    $(SMS++H) $(SDDPBkH) $(BNDSLVH) $(LgDSLVH) $(UCBckH) \
	$(DIR)/CutProcessing.h

# compile command

$(DIR)/sddp_solver.o: $(DIR)/sddp_solver.cpp $(MH)
	$(CC) -c $(DIR)/sddp_solver.cpp -o $@ $(MINC) $(SW)

$(DIR)/sddp_greedy_solver.o: $(DIR)/sddp_greedy_solver.cpp $(MH)
	$(CC) -c $(DIR)/sddp_greedy_solver.cpp -o $@ $(MINC) $(SW)

$(DIR)/CutProcessing.o: $(DIR)/CutProcessing.cpp $(MH)
	$(CC) -c $(DIR)/CutProcessing.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
