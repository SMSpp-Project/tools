# --------------------------------------------------------------------------- #
#    Main CMake file for SMS++ tools                                          #
#                                                                             #
#    This file allows one to build the tests using CMake.                     #
#    To do so, you can use the following commands:                            #
#                                                                             #
#        $ cmake -S <source-path> -B <build-path>                             #
#        $ cmake --build <build-path>                                         #
#                                                                             #
#    Note: this file tries to respect the principles of Modern CMake,         #
#          keep it in mind when editing it.                                   #
#                                                                             #
#                              Niccolo' Iardella                              #
#                                Donato Meoli                                 #
#                         Dipartimento di Informatica                         #
#                             Universita' di Pisa                             #
# --------------------------------------------------------------------------- #
cmake_minimum_required(VERSION 3.19)
cmake_policy(VERSION 3.12)
project(SMS++_tools
        VERSION 0.5.3
        DESCRIPTION "SMS++ tools"
        HOMEPAGE_URL https://gitlab.com/smspp/tools
        LANGUAGES C CXX)

# Find out if it's being called by the umbrella.
get_directory_property(hasParent PARENT_DIRECTORY)

# ----- Settings ------------------------------------------------------------ #
# An option is an ON/OFF user-settable cache variable.
option(tools_USE_DL "Use dynamic loading" OFF)

# This is needed for setting the runtime path when installing.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
include(GNUInstallDirs)

# ----- Tools --------------------------------------------------------------- #
add_subdirectory(block_solver)
add_subdirectory(investment_solver)
add_subdirectory(sddp_solver)
add_subdirectory(ucblock_solver)

add_subdirectory(chgcfg)

# --------------------------------------------------------------------------- #
