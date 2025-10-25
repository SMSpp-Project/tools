##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of tssb_solver                                                  #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = $(DIR)/tssb_solver

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

# TwoStageStochasticBlock
TSSBkSDR = ../../TwoStageStochasticBlock
include $(TSSBkSDR)/makefile-s

# define input macros for UCBlock makefile, then include it
UCBckDIR = ../../UCBlock
include $(UCBckDIR)/lib/makefile

# BundleSolver
BNDSLVSDR = ../../BundleSolver
include $(BNDSLVSDR)/makefile-s

# LagrangianDualSolver
LgDSLVSDR = ../../LagrangianDualSolver
include $(LgDSLVSDR)/makefile

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(SMS++OBJ) $(TSSBkOBJ) $(BNDSLVOBJ) $(LgDSLVOBJ) $(UCBckOBJ)

# libraries
MLIB =  $(SMS++LIB) $(TSSBkLIB) $(BNDSLVLIB) $(LgDSLVLIB) $(UCBckLIB)

$(DIR)/tssb_solver: $(DIR)/tssb_solver.o $(MOBJ)
	$(CC) -o $(DIR)/tssb_solver $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(TSSBkINC) $(BNDSLVINC) $(LgDSLVINC) $(UCBckINC) \
	-I$(DIR) -I../ucblock_solver -I../block_solver

# includes
MH =    $(SMS++H) $(TSSBkH) $(BNDSLVH) $(LgDSLVH) $(UCBckH)

# compile command

$(DIR)/tssb_solver.o: $(DIR)/tssb_solver.cpp \
    $(DIR)/../block_solver/common_utils.h $(MH)
	$(CC) -c $(DIR)/tssb_solver.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
