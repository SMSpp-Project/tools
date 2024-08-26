##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for investment_solver)                                 #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = investment_solver

# basic directory
DIR = .

# common flags
COMMON_SW = -std=c++17 -ferror-limit=1 -Wno-enum-compare -Wno-backslash-newline-escape

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

# BundleSolver
BNDSLVSDR = ../../BundleSolver
include $(BNDSLVSDR)/makefile-c

# not necessary, BundleSolver does that already
# MILPSolver
#MILPSSDR = ../../../MILPSolver
#include $(MILPSSDR)/makefile

# LagrangianDualSolver
LgDSLVSDR = ../../LagrangianDualSolver
include $(LgDSLVSDR)/makefile

# define input macros for UCBlock makefile, then include it
UCBckDIR = ../../UCBlock
include $(UCBckDIR)/lib/makefile

# define input macros for SDDPBlock makefile, then include it
SDDPBkSDR = ../../SDDPBlock
include $(SDDPBkSDR)/makefile-s

# define InvestmentBlock stuff
InvsBkSDR = .

InvsBkOBJ = $(InvsBkSDR)/InvestmentBlock.o \
	$(InvsBkSDR)/InvestmentFunction.o

InvsBkINC = -I$(InvsBkSDR)

InvsBkH   = $(InvsBkSDR)/InvestmentBlock.h \
	$(InvsBkSDR)/InvestmentFunction.h

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(BNDSLVOBJ) $(LgDSLVOBJ) $(UCBckOBJ)  $(SDDPBkOBJ) $(InvsBkOBJ)

# libraries
MLIB =  $(BNDSLVLIB) $(LgDSLVLIB) $(UCBckLIB) $(SDDPBkLIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/investment_solver.o $(DIR)/CutProcessing.o
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

$(InvsBkSDR)/InvestmentBlock.o: $(InvsBkSDR)/InvestmentBlock.cpp \
	$(InvsBkSDR)/InvestmentFunction.h $(SDDPBkH) $(UCBckH) $(SMS++OBJ)
	$(CC) -c $(InvsBkSDR)/InvestmentBlock.cpp -o $@ $(InvsBkINC) \
	$(SDDPBkINC) $(UCBckINC) $(SMS++INC) $(SW)

$(InvsBkSDR)/InvestmentFunction.o: $(InvsBkSDR)/InvestmentFunction.cpp \
	$(InvsBkSDR)/InvestmentFunction.h $(InvsBkSDR)/InvestmentBlock.h \
	$(SDDPBkH) $(UCBckH) $(SMS++OBJ)
	$(CC) -c $(InvsBkSDR)/InvestmentFunction.cpp -o $@ $(InvsBkINC) \
	-I$(DIR)/../sddp_solver -I$(DIR)/../ucblock_solver $(SDDPBkINC) \
	$(UCBckINC) $(SMS++INC) $(SW)

$(DIR)/CutProcessing.o: \
	$(DIR)/../sddp_solver/CutProcessing.cpp $(MH)
	$(CC) -c $(DIR)/../sddp_solver/CutProcessing.cpp \
	-o $(DIR)/CutProcessing.o $(MINC) $(SW)

# include directives
MINC =  $(BNDSLVINC) $(LgDSLVINC) $(UCBckINC) $(SDDPBkINC) \
	-I$(DIR)/../sddp_solver -I$(DIR)/../ucblock_solver

# includes
MH =    $(BNDSLVH) $(LgDSLVH) $(UCBckH) $(SDDPBkH)

# compile command

$(DIR)/investment_solver.o: $(DIR)/investment_solver.cpp \
	$(DIR)/../ucblock_solver/UCBlockSolutionOutput.h \
	$(DIR)/../sddp_solver/SDDPBlockSolutionOutput.h $(MH)
	$(CC) -c $(DIR)/investment_solver.cpp -o $@ $(MINC) $(SW)

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
