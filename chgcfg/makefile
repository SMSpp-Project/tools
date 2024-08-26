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

# common flags
COMMON_SW = -std=c++17 -ferror-limit=1

# debug switches
SW_DEBUG = -g3 -glldb -fno-inline $(COMMON_SW)
# production switches
SW_RELEASE = -O3 $(COMMON_SW) -DNDEBUG

# compiler
CC = clang++

# default target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

BUILD_MODE = release

default: $(BUILD_MODE)

# debug target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

debug: SW = $(SW_DEBUG)
debug: BUILD_MODE = debug
debug: $(DIR)/$(NAME)

# release target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

release: SW = $(SW_RELEASE)
release: BUILD_MODE = release
release: $(DIR)/$(NAME)

# clean target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

clean::
	rm -f $(DIR)/*.o $(DIR)/*~ $(DIR)/$(NAME)

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

$(DIR)/$(NAME): $(DIR)/chgcfg.o
	$(CC) -o $(DIR)/$(NAME) $^ $(SW)

# compile command - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

$(DIR)/chgcfg.o: $(DIR)/chgcfg.cpp
	$(CC) -c $(DIR)/chgcfg.cpp -o $@ $(SW)

# install/uninstall - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

# check if the user is in the sudoers file
ifeq ($(shell id -u),0)
    prefix = /opt/smspp-project
else
    prefix = $(pwd)/smspp-project
endif
bindir = $(prefix)/$(BUILD_MODE)/bin

install: $(DIR)/$(NAME)
	install -s -D $(DIR)/$(NAME) $(DESTDIR)$(bindir)/$(NAME)

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(NAME)

# distclean target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

distclean: clean

# phony targets - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

.PHONY: debug release install uninstall clean distclean

############################ End of makefile #################################
