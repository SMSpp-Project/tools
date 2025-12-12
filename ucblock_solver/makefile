##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for ucblock_solver)                                    #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = ucblock_solver

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

# BundleSolver
BNDSLVSDR = ../../BundleSolver
include $(BNDSLVSDR)/makefile-c

# LagrangianDualSolver
LgDSLVSDR = ../../LagrangianDualSolver
include $(LgDSLVSDR)/makefile

# define input macros for UCBlock makefile, then include it
UCBckDIR = ../../UCBlock
include $(UCBckDIR)/lib/makefile

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(BNDSLVOBJ) $(LgDSLVOBJ) $(UCBckOBJ)

# libraries
MLIB =  $(BNDSLVLIB) $(LgDSLVLIB) $(UCBckLIB)

$(DIR)/$(NAME): $(DIR)/ucblock_solver.o $(MOBJ)
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(BNDSLVINC) $(LgDSLVINC) $(UCBckINC) -I$(DIR)/../block_solver

# includes
MH =    $(BNDSLVH) $(LgDSLVH) $(UCBckH)

# compile command

$(DIR)/ucblock_solver.o: $(DIR)/ucblock_solver.cpp \
	$(DIR)/../block_solver/common_utils.h \
	$(DIR)/UCBlockSolutionOutput.h $(MH)
	$(CC) -c $(DIR)/ucblock_solver.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
