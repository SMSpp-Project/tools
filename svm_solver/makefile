##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of svm_solver                                                   #
#                                                                            #
#                                Donato Meoli                                #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = $(DIR)/svm_solver

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

# note that the SMS++ complete makefile is included in makefile_common

# SVMBlock
SVMBkSDR = ../../SVMBlock
include $(SVMBkSDR)/makefile-s

# BundleSolver, which already includes MILPSolver: these are only needed to
# have a general-purpose and a Lagrangian Solver available besides the ad hoc
# SMOSolver, so comment them out for a build that only trains with the latter
BNDSLVSDR = ../../BundleSolver
include $(BNDSLVSDR)/makefile-s

# LagrangianDualSolver
LgDSLVSDR = ../../LagrangianDualSolver
include $(LgDSLVSDR)/makefile

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(SMS++OBJ) $(SVMBkOBJ) $(BNDSLVOBJ) $(LgDSLVOBJ) \
	$(CMMNUOBJ) $(MLUTOBJ)

# libraries
MLIB =  $(SMS++LIB) $(SVMBkLIB) $(BNDSLVLIB) $(LgDSLVLIB)

$(DIR)/svm_solver: $(DIR)/svm_solver.o $(MOBJ)
	$(CC) -o $(DIR)/svm_solver $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(SVMBkINC) $(BNDSLVINC) $(LgDSLVINC) \
	-I$(DIR) $(CMMNUINC) $(MLUTINC)

# includes
MH =    $(SMS++H) $(SVMBkH) $(BNDSLVH) $(LgDSLVH) $(CMMNUH) $(MLUTH)

# compile command

$(DIR)/svm_solver.o: $(DIR)/svm_solver.cpp \
    $(DIR)/../common_utils.h $(DIR)/../ml_utils.h $(MH)
	$(CC) -c $(DIR)/svm_solver.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
