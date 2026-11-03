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

# define input macros for InvestmentBlock complete makefile, then include it
InvsBkSDR = ../../InvestmentBlock
include $(InvsBkSDR)/makefile-c

# UCBlock, whose units give the design Variable the first-stage paths are
# read against
UCBckDIR = ../../UCBlock
include $(UCBckDIR)/lib/makefile

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

$(DIR)/$(NAME): $(InvsBkOBJ) $(UCBckOBJ) $(DIR)/design_converter.o
	$(CC) -o $(DIR)/$(NAME) $^ $(InvsBkLIB) $(UCBckLIB) $(SW)

# compile command - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

$(DIR)/design_converter.o: $(DIR)/design_converter.cpp $(InvsBkH) $(UCBckH)
	$(CC) -c $(DIR)/design_converter.cpp -o $@ $(InvsBkINC) $(UCBckINC) \
	$(SW)

############################ End of makefile #################################
