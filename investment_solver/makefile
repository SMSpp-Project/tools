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

# common stuff
include ../makefile_common

# define & include the necessary modules- - - - - - - - - - - - - - - - - - -
# if a module is not used in the current configuration, just comment out the
# corresponding include line
# each module outputs some macros to be used here:
# *OBJ is the final object(s) / library
# *LIB is the external libraries + -L< libdirs >
# *H   is the list of all include files
# *INC is the -I< include directories >

# note that the SMS++ complete makefile isincluded in makefile_common

# define input macros for SDDPBlock makefile, then include it
SDDPBkSDR = ../../SDDPBlock
include $(SDDPBkSDR)/makefile-s

# define input macros for UCBlock makefile, then include it
UCBckDIR = ../../UCBlock
include $(UCBckDIR)/lib/makefile

# BundleSolver
BNDSLVSDR = ../../BundleSolver
include $(BNDSLVSDR)/makefile-s

# not necessary, BundleSolver does that already
# MILPSolver
#MILPSSDR = ../../../MILPSolver
#include $(MILPSSDR)/makefile

# LagrangianDualSolver
LgDSLVSDR = ../../LagrangianDualSolver
include $(LgDSLVSDR)/makefile

# define InvestmentBlock stuff
InvsBkSDR = .

InvsBkOBJ = $(InvsBkSDR)/InvestmentBlock.o \
	$(InvsBkSDR)/InvestmentFunction.o

InvsBkINC = -I$(InvsBkSDR)

InvsBkH   = $(InvsBkSDR)/InvestmentBlock.h \
	$(InvsBkSDR)/InvestmentFunction.h

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(SMS++OBJ) $(UCBckOBJ) $(SDDPBkOBJ) $(BNDSLVOBJ) $(LgDSLVOBJ) \
	$(InvsBkOBJ) $(CMMNUOBJ)

# libraries
MLIB =   $(SMS++LIB) $(UCBckLIB) $(SDDPBkLIB) $(BNDSLVLIB) $(LgDSLVLIB)

$(DIR)/$(NAME): $(DIR)/investment_solver.o $(DIR)/CutProcessing.o $(MOBJ)
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(BNDSLVINC) $(LgDSLVINC) $(UCBckINC) $(SDDPBkINC) \
	-I$(DIR)/../sddp_solver -I$(DIR)/../ucblock_solver $(CMMNUINC)

# includes
MH =    $(SMS++H) $(BNDSLVH) $(LgDSLVH) $(UCBckH) $(SDDPBkH) $(CMMNUH)

# compile command

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

$(DIR)/investment_solver.o: $(DIR)/investment_solver.cpp \
	$(DIR)/../ucblock_solver/UCBlockSolutionOutput.h \
	$(DIR)/../sddp_solver/SDDPBlockSolutionOutput.h $(MH)
	$(CC) -c $(DIR)/investment_solver.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
