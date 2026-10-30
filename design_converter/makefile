##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of design_converter                                             #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = design_converter

# basic directory
DIR = .

# common stuff
include ../makefile_common

# define input macros for InvestmentBlock complete makefile, then include it;
# it brings UCBlock, whose units give the design Variable the first-stage
# paths are read against
InvsBkSDR = ../../InvestmentBlock
include $(InvsBkSDR)/makefile-c

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

$(DIR)/$(NAME): $(InvsBkOBJ) $(DIR)/design_converter.o
	$(CC) -o $(DIR)/$(NAME) $^ $(InvsBkLIB) $(SW)

# compile command - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

$(DIR)/design_converter.o: $(DIR)/design_converter.cpp $(InvsBkH)
	$(CC) -c $(DIR)/design_converter.cpp -o $@ $(InvsBkINC) $(SW)

############################ End of makefile #################################
