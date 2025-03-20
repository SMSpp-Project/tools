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

# common flags
COMMON_SW = -std=c++20 -ferror-limit=1 -Wno-enum-compare

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
debug: $(DIR)/$(NAME)

# debug_asan target - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

debug_asan: SW = $(SW_DEBUG_ASAN)
debug_asan: BUILD_MODE = debug
debug_asan: $(DIR)/$(NAME)

# release target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

release: SW = $(SW_RELEASE)
release: BUILD_MODE = release
release: $(DIR)/$(NAME)

# release_asan target - - - - - - - - - - - - - - - - - - - - - - - - - - - -

release_asan: SW = $(SW_RELEASE_ASAN)
release_asan: BUILD_MODE = release
release_asan: $(DIR)/$(NAME)

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

# not necessary, BundleSolver does that already
# MILPSolver
#MILPSSDR = ../../MILPSolver
#include $(MILPSSDR)/makefile-c

# define input macros for UCBlock makefile, then include it
UCBckDIR = ../../UCBlock
include $(UCBckDIR)/lib/makefile

# MCFBlock
MCFBkSDR = ../../MCFBlock
include $(MCFBkSDR)/makefile-s

# LEMONSolver, assuming all dependencies (MCFBlock) are there already
# (comment out if not required)
# note: -std=c++20 is required if this is included
MCFLESDR = ../../LEMONSolver
include $(MCFLESDR)/makefile

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(BNDSLVOBJ) $(LgDSLVOBJ) $(UCBckOBJ) $(SMS++OBJ) $(MCFLEOBJ) \
	$(MCFBkOBJ)

# libraries
MLIB =  $(SMS++LIB) $(BNDSLVLIB) $(LgDSLVLIB) $(UCBckLIB) $(MCFBkLIB) \
	$(MILPSLIB)

$(DIR)/$(NAME): $(DIR)/block_solver.o $(MOBJ)
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(BNDSLVINC) $(LgDSLVINC) $(UCBckINC) $(MCFBkINC) \
	$(MCFLEINC)

# includes
MH =    $(SMS++H) $(BNDSLVH) $(LgDSLVH) $(UCBckH) $(MCFBkH) $(MCFLEH)

# compile command

$(DIR)/block_solver.o: $(DIR)/block_solver.cpp $(DIR)/common_utils.h $(MH)
	$(CC) -c $(DIR)/block_solver.cpp -o $@ $(MINC) $(SW)

# install/uninstall - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

# check if the user is in the sudoers file
ifeq ($(shell id -u),0)
    prefix = /opt/smspp-project
else
    prefix = $(HOME)/smspp-project
endif
bindir = $(prefix)/$(BUILD_MODE)/bin

install: $(DIR)/$(NAME)
	install -s -D $(DIR)/$(NAME) $(DESTDIR)$(bindir)/$(NAME)

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(NAME)

# distclean target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

distclean: clean

# phony targets - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

.PHONY: debug debug_asan release release_asan install uninstall clean distclean

############################ End of makefile #################################
