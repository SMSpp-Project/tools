##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of sddp_solver                                                  #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# basic directory
DIR = .

# module name
NAME = $(DIR)/sddp_solver

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

# BundleSolver
BNDSLVSDR = ../../BundleSolver
include $(BNDSLVSDR)/makefile-s

# LagrangianDualSolver
LgDSLVSDR = ../../LagrangianDualSolver
include $(LgDSLVSDR)/makefile

# define input macros for SDDPBlock makefile + dependencies, then include it
SDDPBkSDR = ../../SDDPBlock
include $(SDDPBkSDR)/makefile-s

# define input macros for UCBlock makefile, then include it
UCBckDIR = ../../UCBlock
include $(UCBckDIR)/lib/makefile

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(SMS++OBJ) $(SDDPBkOBJ) $(BNDSLVOBJ) $(LgDSLVOBJ) $(UCBckOBJ) \
	$(CMMNUOBJ)

# libraries
MLIB =  $(SMS++LIB) $(SDDPBkLIB) $(BNDSLVLIB) $(LgDSLVLIB) $(UCBckLIB)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(SDDPBkINC) $(BNDSLVINC) $(LgDSLVINC) $(UCBckINC) \
	-I$(DIR) $(CMMNUINC)

# includes
MH =    $(SMS++H) $(SDDPBkH) $(BNDSLVH) $(LgDSLVH) $(UCBckH) $(CMMNUH)

# compile commands

$(DIR)/sddp_solver.o: $(DIR)/sddp_solver.cpp $(MH)
	$(CC) -c $(DIR)/sddp_solver.cpp -o $@ $(MINC) $(SW)

$(DIR)/sddp_solver: $(DIR)/sddp_solver.o $(MOBJ)
	$(CC) -o $(DIR)/sddp_solver $^ $(MLIB) -I.. $(SW)

############################ End of makefile #################################
