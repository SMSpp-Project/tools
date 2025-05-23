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

# basic directory
DIR = .

# module name
NAME = $(DIR)/tssb_solver

# common flags
COMMON_SW = -std=c++20 -ferror-limit=1 -Wno-enum-compare -Wno-backslash-newline-escape

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
debug: $(DIR)/tssb_solver

# debug_asan target - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

debug_asan: SW = $(SW_DEBUG_ASAN)
debug_asan: BUILD_MODE = debug
debug_asan: $(DIR)/tssb_solver

# release target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

release: SW = $(SW_RELEASE)
release: BUILD_MODE = release
release: $(DIR)/tssb_solver

# release_asan target - - - - - - - - - - - - - - - - - - - - - - - - - - - -

release_asan: SW = $(SW_RELEASE_ASAN)
release_asan: BUILD_MODE = release_asan
release_asan: $(DIR)/tssb_solver

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

# define input macros for TwoStageStochasticBlock makefile + dependencies, then include it
TSSBkSDR = ../../TwoStageStochasticBlock
include $(TSSBkSDR)/makefile-s

# define input macros for UCBlock makefile, then include it
UCBckDIR = ../../UCBlock
include $(UCBckDIR)/lib/makefile

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

# install/uninstall - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

# check if the user is in the sudoers file
ifeq ($(shell id -u),0)
    prefix = /opt/smspp-project
else
    prefix = $(HOME)/smspp-project
endif
bindir = $(prefix)/$(BUILD_MODE)/bin

install: $(DIR)/tssb_solver
	install -s -D $(DIR)/tssb_solver $(DESTDIR)$(bindir)/tssb_solver

uninstall:
	rm -f $(DESTDIR)$(bindir)/tssb_solver

# distclean target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

distclean: clean

# phony targets - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

.PHONY: debug debug_asan release release_asan install uninstall clean distclean

############################ End of makefile #################################
