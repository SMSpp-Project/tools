##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of sddp_solver and sddp_greedy_solver                           #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# basic directory
DIR = .

# module name
NAME = $(DIR)/sddp_solver $(DIR)/sddp_greedy_solver

# common flags
COMMON_SW = -std=c++17 -ferror-limit=1 -Wno-enum-compare -Wno-unqualified-std-cast-call -Wno-backslash-newline-escape

# debug switches
SW_DEBUG = -g3 -glldb -fno-inline $(COMMON_SW)
# debug switches with address sanitizer and extra pedantic warning
SW_DEBUG_ASAN = -g3 -glldb -fno-inline $(COMMON_SW) -fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer -Wpedantic -Wextra -Wno-unused-parameter -DCLANG_1200_0_32_27_PATCH
# production switches with address sanitizer
SW_RELEASE_ASAN = -O3 $(COMMON_SW) -DNDEBUG -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined
# production switches
SW_RELEASE = -O3 $(COMMON_SW) -DNDEBUG -DCLANG_1200_0_32_27_PATCH

# compiler
CC = clang++

# default target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

BUILD_MODE = release

default: $(BUILD_MODE)

# debug target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

debug: SW = $(SW_DEBUG)
debug: BUILD_MODE = debug
debug: $(DIR)/sddp_solver $(DIR)/sddp_greedy_solver

# debug_asan target - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

debug_asan: SW = $(SW_DEBUG_ASAN)
debug_asan: BUILD_MODE = debug
debug_asan: $(DIR)/sddp_solver $(DIR)/sddp_greedy_solver

# release target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

release: SW = $(SW_RELEASE)
release: BUILD_MODE = release
release: $(DIR)/sddp_solver $(DIR)/sddp_greedy_solver

# release_asan target - - - - - - - - - - - - - - - - - - - - - - - - - - - -

release_asan: SW = $(SW_RELEASE_ASAN)
release_asan: BUILD_MODE = release_asan
release_asan: $(DIR)/sddp_solver $(DIR)/sddp_greedy_solver

# clean target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

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

# define input macros for SMS++ complete makefile, then include it
SMS++SDR = ../../SMS++
include $(SMS++SDR)/lib/makefile-c

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
	$(DIR)/CutProcessing.o

# libraries
MLIB =  $(SMS++LIB) $(SDDPBkLIB) $(BNDSLVLIB) $(LgDSLVLIB) $(UCBckLIB)

$(DIR)/sddp_solver: $(DIR)/sddp_solver.o $(MOBJ)
	$(CC) -o $(DIR)/sddp_solver $^ $(MLIB) $(SW)

$(DIR)/sddp_greedy_solver: $(DIR)/sddp_greedy_solver.o $(MOBJ)
	$(CC) -o $(DIR)/sddp_greedy_solver $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(SDDPBkINC) $(BNDSLVINC) $(LgDSLVINC) $(UCBckINC) \
	-I$(DIR) -I../ucblock_solver

# includes
MH =    $(SMS++H) $(SDDPBkH) $(BNDSLVH) $(LgDSLVH) $(UCBckH) \
	$(DIR)/CutProcessing.h

# compile command

$(DIR)/sddp_solver.o: $(DIR)/sddp_solver.cpp $(MH)
	$(CC) -c $(DIR)/sddp_solver.cpp -o $@ $(MINC) $(SW)

$(DIR)/sddp_greedy_solver.o: $(DIR)/sddp_greedy_solver.cpp $(MH)
	$(CC) -c $(DIR)/sddp_greedy_solver.cpp -o $@ $(MINC) $(SW)

$(DIR)/CutProcessing.o: $(DIR)/CutProcessing.cpp $(MH)
	$(CC) -c $(DIR)/CutProcessing.cpp -o $@ $(MINC) $(SW)

# install/uninstall - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

prefix = /opt/smspp-project
bindir = $(prefix)/$(BUILD_MODE)/bin

install: $(DIR)/sddp_solver $(DIR)/sddp_greedy_solver
	install -s -D $(DIR)/sddp_solver $(DESTDIR)$(bindir)/sddp_solver
	install -s -D $(DIR)/sddp_greedy_solver $(DESTDIR)$(bindir)/sddp_greedy_solver

uninstall:
	rm -f $(DESTDIR)$(bindir)/sddp_solver
	rm -f $(DESTDIR)$(bindir)/sddp_greedy_solver

# distclean target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

distclean: clean

# phony targets - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

.PHONY: debug debug_asan release release_asan install uninstall clean distclean

############################ End of makefile #################################
