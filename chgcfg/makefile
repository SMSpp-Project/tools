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

# debug switches
#SW = -g3 -glldb -fno-inline -std=c++17 -ferror-limit=1
# production switches
SW = -O3 -std=c++17 -DNDEBUG -ferror-limit=1

# compiler
CC = clang++

# default target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

default: $(DIR)/$(NAME)

# clean - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

clean::
	rm -f $(DIR)/*.o $(DIR)/*~ $(DIR)/$(NAME)

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

$(DIR)/$(NAME): $(DIR)/chgcfg.o
	$(CC) -o $(DIR)/$(NAME) $^ $(SW)

# compile command - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

$(DIR)/chgcfg.o: $(DIR)/chgcfg.cpp
	$(CC) -c $(DIR)/chgcfg.cpp -o $@ $(SW)

############################ End of makefile #################################
