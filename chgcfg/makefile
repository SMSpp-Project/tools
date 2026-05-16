##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of chgcfg                                                       #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = chgcfg

# basic directory
DIR = .

# common stuff
include ../makefile_common

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(SMS++OBJ) $(CMMNUOBJ)

# libraries
MLIB =  $(SMS++LIB)

$(DIR)/$(NAME): $(DIR)/chgcfg.o $(MOBJ)
	$(CC) -o $(DIR)/$(NAME) $+ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(CMMNUINC)

# includes
MH =    $(SMS++H) $(CMMNUH)

# compile command - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

$(DIR)/chgcfg.o: $(DIR)/chgcfg.cpp $(MH)
	$(CC) -c $(DIR)/chgcfg.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
