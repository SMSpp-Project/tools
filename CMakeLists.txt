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
#                          Operations Research Group                          #
#                         Dipartimento di Informatica                         #
#                             Universita' di Pisa                             #
# --------------------------------------------------------------------------- #
cmake_minimum_required(VERSION 3.11)
project(SMS++_tools
        VERSION 0.3.1
        DESCRIPTION "SMS++ tools"
        HOMEPAGE_URL https://gitlab.com/smspp/tools
        LANGUAGES C CXX)

# ----- Settings ------------------------------------------------------------ #
# An option is an ON/OFF user-settable cache variable.
option(tools_USE_DL "Use dynamic loading" ON)

# This is needed for setting the runtime path when installing.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
include(GNUInstallDirs)

# Since we are using the block factory, objects from statically linked libraries
# may wrongly appear unused, and by default the linker does not include them,
# so we have to force the linking.
if (NOT BUILD_SHARED_LIBS)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        add_link_options("-Wl,-all_load")
    else ()
        add_link_options("-Wl,--whole-archive,--allow-multiple-definition")
    endif ()
endif ()

# ----- Tools --------------------------------------------------------------- #
add_subdirectory(block_solver)
add_subdirectory(sddp_solver)
add_subdirectory(thermalunit_solver)
add_subdirectory(ucblock_solver)

# --------------------------------------------------------------------------- #
# Just consuming these to avoid warnings
if (BUILD_TESTING)
endif ()

if (CMAKE_EXPORT_PACKAGE_REGISTRY)
endif ()

# --------------------------------------------------------------------------- #
