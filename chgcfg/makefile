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

# common_utils and SMS++, whose Configuration it reads and writes
$(DIR)/$(NAME): $(DIR)/chgcfg.o $(CMMNUOBJ) $(SMS++OBJ)
	$(CC) -o $(DIR)/$(NAME) $^ $(SMS++LIB) $(SW)

# compile command - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

$(DIR)/chgcfg.o: $(DIR)/chgcfg.cpp $(CMMNUH) $(SMS++H)
	$(CC) -c $(DIR)/chgcfg.cpp -o $@ $(CMMNUINC) $(SMS++INC) $(SW)

############################ End of makefile #################################
