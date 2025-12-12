##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for block_solver)                                      #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = block_solver

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

# define input macros for SMS++ complete makefile, then include it
SMS++SDR = ../../SMS++
include $(SMS++SDR)/lib/makefile-c

# MCFBlock
# this has to be before BundleSolver because there are two definitions of
# OPTstuff, but the one in MCFBlock lacks OPTvect.h that is needed by
# the NDOstuff in BundleSolver
MCFBkSDR = ../../MCFBlock
include $(MCFBkSDR)/makefile-s

# BundleSolver
BNDSLVSDR = ../../BundleSolver
include $(BNDSLVSDR)/makefile-s

# LagrangianDualSolver
LgDSLVSDR = ../../LagrangianDualSolver
include $(LgDSLVSDR)/makefile

# not necessary, BundleSolver does that already
# MILPSolver
#MILPSSDR = ../../MILPSolver
#include $(MILPSSDR)/makefile-c

# define input macros for UCBlock makefile, then include it
# not necessary, InvestmentBlock does it already
#UCBckDIR = ../../UCBlock
#include $(UCBckDIR)/lib/makefile

# LEMONSolver, assuming all dependencies (MCFBlock) are there already
# (comment out if not required)
# note: -std=c++20 is required if this is included
MCFLESDR = ../../LEMONSolver
include $(MCFLESDR)/makefile

# InvestmentBlock
InvsBkSDR = ../../InvestmentBlock
include $(InvsBkSDR)/makefile-s

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(BNDSLVOBJ) $(LgDSLVOBJ) $(UCBckOBJ) $(SMS++OBJ) $(MCFLEOBJ) \
	$(MCFBkOBJ) $(InvsBkOBJ)

# libraries
MLIB =  $(SMS++LIB) $(BNDSLVLIB) $(LgDSLVLIB) $(UCBckLIB) $(MCFBkLIB) \
	$(MILPSLIB) $(InvsBkLIB)

$(DIR)/$(NAME): $(DIR)/block_solver.o $(MOBJ)
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(BNDSLVINC) $(LgDSLVINC) $(UCBckINC) $(MCFBkINC) \
	$(MCFLEINC) $(InvsBkINC)

# includes
MH =    $(SMS++H) $(BNDSLVH) $(LgDSLVH) $(UCBckH) $(MCFBkH) $(MCFLEH) \
	$(InvsBkH)

# compile command

$(DIR)/block_solver.o: $(DIR)/block_solver.cpp $(DIR)/common_utils.h $(MH)
	$(CC) -c $(DIR)/block_solver.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
