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

$(DIR)/$(NAME): $(DIR)/chgcfg.o
	$(CC) -o $(DIR)/$(NAME) $^ $(SW)

# compile command - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

$(DIR)/chgcfg.o: $(DIR)/chgcfg.cpp
	$(CC) -c $(DIR)/chgcfg.cpp -o $@ $(SW)

############################ End of makefile #################################
