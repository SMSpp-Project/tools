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
DIR = ./

# debug switches
#SW = -g3 -glldb -fno-inline -std=c++17 -ferror-limit=1 -Wno-enum-compare
# debug switches with address sanitizer and extra pedantic warning
#SW = -g3 -glldb -fno-inline -std=c++17 -ferror-limit=1 -fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer -Wpedantic -Wextra -Wno-unused-parameter -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare
# production switches with address sanitizer
#SW = -O3 -std=c++17 -DNDEBUG -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -Wno-enum-compare
# production switches
SW = -O3 -std=c++17 -DNDEBUG -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare

# linker switches
#LSW = -std=c++17 -ferror-limit=1

# compiler
CC = clang++

# default target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

default: $(NAME)

# clean - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

clean::
	rm -f $(DIR)*.o $(DIR)*~ $(NAME)

# define & include the necessary modules- - - - - - - - - - - - - - - - - - -
# if a module is not used in the current configuration, just comment out the
# corresponding include line
# each module outputs some macros to be used here:
# *OBJ is the final object(s) / library
# *LIB is the external libreries + -L<libdirs>
# *H   is the list of all include files
# *INC is the -I<include directories>

# BundleSolver
BNDSLVSDR = ../../BundleSolver/
include $(BNDSLVSDR)makefile-c

# LagrangianDualSolver
LgDSLVSDR = ../../LagrangianDualSolver/
include $(LgDSLVSDR)makefile

# define input macros for UCBlock makefile, then include it
# note that this is not the "complete" makefile: UCBlock only requires
# the core SMS++, that is already included above (by LagrangianDualSolver)
UCBckDIR = ../../UCBlock/
include $(UCBckDIR)lib/makefile

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(BNDSLVOBJ) $(LgDSLVOBJ) $(UCBckOBJ)

# libreries
MLIB =  $(BNDSLVLIB) $(LgDSLVLIB) $(UCBckLIB)

$(NAME): $(MOBJ) $(DIR)ucblock_solver.o
	$(CC) -o $(DIR)$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .C + every recursively included .h- - - - -

# include directives
MINC =  $(BNDSLVINC) $(LgDSLVINC) $(UCBckINC) -I../block_solver/

# includes
MH =    $(BNDSLVH) $(LgDSLVH) $(UCBckH)

# compile command

$(DIR)ucblock_solver.o: $(DIR)ucblock_solver.cpp $(MH)
	$(CC) -c $(DIR)ucblock_solver.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
