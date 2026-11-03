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

# the Blocks the inner Block of an InvestmentBlock is made of, which it
# reaches polymorphically and this program names: UCBlock, SDDPBlock (with
# StochasticBlock) and the basic makefile of TwoStageStochasticBlock, since
# StochasticBlock comes with SDDPBlock already
UCBckDIR = ../../UCBlock
include $(UCBckDIR)/lib/makefile
SDDPBkSDR = ../../SDDPBlock
include $(SDDPBkSDR)/makefile-s
TSSBkSDR = ../../TwoStageStochasticBlock
include $(TSSBkSDR)/makefile

# the multi-stage stochastic Block has to be asked for too, the Block being
# reached polymorphically and only the factory needing to know how to build
# it; the basic makefile, since TwoStageStochasticBlock is there already
MSSBkSDR = ../../MultiStageStochasticBlock
include $(MSSBkSDR)/makefile

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
MOBJ =  $(BNDSLVOBJ) $(LgDSLVOBJ) $(InvsBkOBJ) $(UCBckOBJ) $(SDDPBkOBJ) \
	$(TSSBkOBJ) $(MSSBkOBJ) $(CMMNUOBJ)

# libraries
MLIB =  $(BNDSLVLIB) $(LgDSLVLIB) $(InvsBkLIB) $(UCBckLIB) $(SDDPBkLIB) \
	$(TSSBkLIB) $(MSSBkLIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/investmentblock_solver.o
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(BNDSLVINC) $(LgDSLVINC) $(InvsBkINC) $(UCBckINC) $(SDDPBkINC) \
	$(TSSBkINC) $(CMMNUINC)

# includes
MH =    $(BNDSLVH) $(LgDSLVH) $(InvsBkH) $(UCBckH) $(SDDPBkH) $(TSSBkH) \
	$(CMMNUH)

# compile command

$(DIR)/investmentblock_solver.o: $(DIR)/investmentblock_solver.cpp $(MH)
	$(CC) -c $(DIR)/investmentblock_solver.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
