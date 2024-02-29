##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for investment_solver)                                 #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = investment_solver

# basic directory
DIR = .

# debug switches
#SW = -g3 -glldb -fno-inline -std=c++17 -ferror-limit=1 -Wno-enum-compare
# debug switches with address sanitizer and extra pedantic warning
#SW = -g3 -glldb -fno-inline -std=c++17 -ferror-limit=1 -fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer -Wpedantic -Wextra -Wno-unused-parameter -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare
# production switches with address sanitizer
#SW = -O3 -std=c++17 -DNDEBUG -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -Wno-enum-compare
# production switches
SW = -O3 -std=c++17 -DNDEBUG -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare

# linker switches
#LSW = -std=c++17 -ferror-limit=1

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

# BundleSolver
BNDSLVSDR = ../../BundleSolver
include $(BNDSLVSDR)/makefile-c

# not necessary, BundleSolver does that already
# MILPSolver
#MILPSSDR = ../../../MILPSolver
#include $(MILPSSDR)/makefile

# LagrangianDualSolver
LgDSLVSDR = ../../LagrangianDualSolver
include $(LgDSLVSDR)/makefile

# define input macros for UCBlock makefile, then include it
UCBckDIR = ../../UCBlock
include $(UCBckDIR)/lib/makefile

# define input macros for SDDPBlock makefile, then include it
SDDPBkSDR = ../../SDDPBlock
include $(SDDPBkSDR)/makefile-s

# define InvestmentBlock stuff
InvsBkSDR = .

InvsBkOBJ = $(InvsBkSDR)/InvestmentBlock.o \
	$(InvsBkSDR)/InvestmentFunction.o

InvsBkINC = -I$(InvsBkSDR)

InvsBkH   = $(InvsBkSDR)/InvestmentBlock.h \
	$(InvsBkSDR)/InvestmentFunction.h

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(BNDSLVOBJ) $(LgDSLVOBJ) $(UCBckOBJ)  $(SDDPBkOBJ) $(InvsBkOBJ)

# libraries
MLIB =  $(BNDSLVLIB) $(LgDSLVLIB) $(UCBckLIB) $(SDDPBkLIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/investment_solver.o $(DIR)/CutProcessing.o
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .C + every recursively included .h- - - - -

$(InvsBkSDR)/InvestmentBlock.o: $(InvsBkSDR)/InvestmentBlock.cpp \
	$(InvsBkSDR)/InvestmentFunction.h $(SDDPBkH) $(UCBckH) $(SMS++OBJ)
	$(CC) -c $(InvsBkSDR)/InvestmentBlock.cpp -o $@ $(InvsBkINC) \
	$(SDDPBkINC) $(UCBckINC) $(SMS++INC) $(SW)

$(InvsBkSDR)/InvestmentFunction.o: $(InvsBkSDR)/InvestmentFunction.cpp \
	$(InvsBkSDR)/InvestmentFunction.h $(InvsBkSDR)/InvestmentBlock.h \
	$(SDDPBkH) $(UCBckH) $(SMS++OBJ)
	$(CC) -c $(InvsBkSDR)/InvestmentFunction.cpp -o $@ $(InvsBkINC) \
	-I$(DIR)/../sddp_solver -I$(DIR)/../ucblock_solver $(SDDPBkINC) \
	$(UCBckINC) $(SMS++INC) $(SW)

$(DIR)/CutProcessing.o: \
	$(DIR)/../sddp_solver/CutProcessing.cpp $(MH)
	$(CC) -c $(DIR)/../sddp_solver/CutProcessing.cpp \
	-o $(DIR)/CutProcessing.o $(MINC) $(SW)

# include directives
MINC =  $(BNDSLVINC) $(LgDSLVINC) $(UCBckINC) $(SDDPBkINC) \
	-I$(DIR)/../sddp_solver -I$(DIR)/../ucblock_solver

# includes
MH =    $(BNDSLVH) $(LgDSLVH) $(UCBckH) $(SDDPBkH)

# compile command

$(DIR)/investment_solver.o: $(DIR)/investment_solver.cpp \
	$(DIR)/../ucblock_solver/UCBlockSolutionOutput.h \
	$(DIR)/../sddp_solver/SDDPBlockSolutionOutput.h $(MH)
	$(CC) -c $(DIR)/investment_solver.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
