##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for InvestmentBlock)                                   #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = investmentblock_solver

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

# define input macros for InvestmentBlock complete makefile, then include it
InvsBkSDR = ../../InvestmentBlock
include $(InvsBkSDR)/makefile-c

# define input macros for BundleSolver makefile, then include it
BNDSLVSDR = ../../BundleSolver
include $(BNDSLVSDR)/makefile-s

# define input macros for LagrangianDualSolver makefile, then include it
LgDSLVSDR = ../../LagrangianDualSolver
include $(LgDSLVSDR)/makefile

# not necessary, BundleSolver does that already
# MILPSolver
#MILPSSDR = ../../MILPSolver
#include $(MILPSSDR)/makefile-c

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(BNDSLVOBJ) $(LgDSLVOBJ) $(InvsBkOBJ) $(CMMNUOBJ)

# libraries
MLIB =  $(BNDSLVLIB) $(LgDSLVLIB) $(InvsBkLIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/investmentblock_solver.o
	$(CC) -o $(DIR)/$(NAME) $+ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(BNDSLVINC) $(LgDSLVINC) $(InvsBkINC) $(CMMNUINC)

# includes
MH =    $(BNDSLVH) $(LgDSLVH) $(InvsBkH) $(CMMNUH)

# compile command

$(DIR)/investmentblock_solver.o: $(DIR)/investmentblock_solver.cpp $(MH)
	$(CC) -c $(DIR)/investmentblock_solver.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
