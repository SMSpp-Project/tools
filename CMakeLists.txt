# --------------------------------------------------------------------------- #
#    Main CMake file for SMS++ tools                                          #
#                                                                             #
#    This file allows one to build the tests using CMake.                     #
#    To do so, you can use the following commands:                            #
#                                                                             #
#        $ cmake -S <source-path> -B <build-path>                             #
#        $ cmake --build <build-path>                                         #
#                                                                             #
#                                Donato Meoli                                 #
#                         Dipartimento di Informatica                         #
#                             Universita' di Pisa                             #
# --------------------------------------------------------------------------- #
cmake_minimum_required(VERSION 3.21)
cmake_policy(VERSION 3.15)
project(SMS++_tools
        VERSION 0.5.4
        DESCRIPTION "SMS++ tools"
        HOMEPAGE_URL https://gitlab.com/smspp/tools
        LANGUAGES C CXX)

# Find out if it's being called by the umbrella.
get_directory_property(hasParent PARENT_DIRECTORY)

# Utility function to standardize installation and runtime dependency handling
# for SMS++ tools.
function(smspp_install target_name)
    if (WIN32 AND BUILD_SHARED_LIBS AND NOT DEFINED ENV{CONDA_BUILD})
        # On Windows with shared libraries enabled copies all runtime DLL
        # dependencies next to the executable in the build directory
        # (POST_BUILD), using CMake's TARGET_RUNTIME_DLLS generator expression
        add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target_name}>
                $<TARGET_FILE_DIR:${target_name}>
                COMMAND_EXPAND_LISTS
                COMMENT "Copying runtime DLLs next to ${target_name}.exe")
    endif ()

    install(TARGETS ${target_name}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endfunction()

# ----- Settings ------------------------------------------------------------ #
# This is needed for setting the runtime path when installing.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
include(GNUInstallDirs)

# ----- Tools --------------------------------------------------------------- #
add_subdirectory(block_solver)
add_subdirectory(investmentblock_solver)
add_subdirectory(sddp_solver)
add_subdirectory(tssb_solver)
add_subdirectory(mssb_solver)
add_subdirectory(ucblock_solver)

add_subdirectory(chgcfg)

# --------------------------------------------------------------------------- #
