##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of mssb_solver                                                  #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = $(DIR)/mssb_solver

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

# MultiStageStochasticBlock
MSSBkSDR = ../../MultiStageStochasticBlock
include $(MSSBkSDR)/makefile-s

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
MOBJ =  $(SMS++OBJ) $(MSSBkOBJ) $(BNDSLVOBJ) $(LgDSLVOBJ) $(UCBckOBJ) \
	$(CMMNUOBJ)

# libraries
MLIB =  $(SMS++LIB) $(MSSBkLIB) $(BNDSLVLIB) $(LgDSLVLIB) $(UCBckLIB)

$(DIR)/mssb_solver: $(DIR)/mssb_solver.o $(MOBJ)
	$(CC) -o $(DIR)/mssb_solver $^ $(MLIB) $(SW)

# par_build_test: construction (de-serialization) micro-benchmark, see
# par_build_test.cpp; reuses the same objects/libraries as mssb_solver
$(DIR)/par_build_test: $(DIR)/par_build_test.o $(MOBJ)
	$(CC) -o $(DIR)/par_build_test $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(MSSBkINC) $(BNDSLVINC) $(LgDSLVINC) $(UCBckINC) \
	-I$(DIR) -I../ucblock_solver $(CMMNUINC)

# includes
MH =    $(SMS++H) $(MSSBkH) $(BNDSLVH) $(LgDSLVH) $(UCBckH) $(CMMNUH)

# compile command

$(DIR)/mssb_solver.o: $(DIR)/mssb_solver.cpp \
    $(DIR)/../common_utils.h $(MH)
	$(CC) -c $(DIR)/mssb_solver.cpp -o $@ $(MINC) $(SW)

$(DIR)/par_build_test.o: $(DIR)/par_build_test.cpp $(MH)
	$(CC) -c $(DIR)/par_build_test.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
