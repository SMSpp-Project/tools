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
# for SMS++ tools (executables).
function(smspp_install target_name)
    if (WIN32 AND BUILD_SHARED_LIBS AND NOT DEFINED ENV{CONDA_BUILD})
        # On Windows with shared libraries enabled:
        # 1. copies all runtime DLL dependencies next to the executable in the
        #    build directory (POST_BUILD), using CMake's TARGET_RUNTIME_DLLS
        #    generator expression
        # 2. installs the executable and automatically resolves its runtime
        #    dependencies using RUNTIME_DEPENDENCY_SET
        # 3. provides additional search paths (DIRECTORIES) to help CMake locate
        #    transitive runtime dependencies (e.g., solver DLLs such as SCIP,
        #    CPLEX and GUROBI) that are not always discoverable automatically
        # 4. filters out system and irrelevant DLLs via regex exclusions
        add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target_name}>
                $<TARGET_FILE_DIR:${target_name}>
                COMMAND_EXPAND_LISTS
                COMMENT "Copying runtime DLLs next to ${target_name}.exe")

        install(TARGETS ${target_name}
                RUNTIME_DEPENDENCY_SET ${target_name}_deps
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

        install(RUNTIME_DEPENDENCY_SET ${target_name}_deps
                DESTINATION ${CMAKE_INSTALL_BINDIR}
                # This is needed since some third-party libraries
                # (e.g., SCIP + TBB) expose runtime dependencies that are not
                # fully described via CMake targets (missing import libraries or
                # incomplete metadata). In these cases, CMake cannot
                # automatically resolve all DLLs during installation unless
                # explicit search directories are provided
                DIRECTORIES
                "${SCIP_ROOT}/bin"
                "${GUROBI_ROOT}/win64/bin"
                "${CPLEX_ROOT}/cplex/bin/x64_win64"
                "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
                PRE_EXCLUDE_REGEXES
                "api-ms-.*"
                "ext-ms-.*"
                "^azureattestmanager\\.dll$"
                "^azureattestnormal\\.dll$"
                "^hvsifiletrust\\.dll$"
                "^pdmutilities\\.dll$"
                "^wpaxholder\\.dll$"
                POST_EXCLUDE_REGEXES
                ".*[\\/]system32[\\/].*"
                ".*[\\/]windows[\\/].*")
    else ()
        # On other platforms, or Windows with static builds, performs a standard
        # installation of the executable without additional runtime dependency
        # handling
        install(TARGETS ${target_name}
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif ()
endfunction()

# ----- Settings ------------------------------------------------------------ #
# This is needed for setting the runtime path when installing.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
include(GNUInstallDirs)

# ----- Tools --------------------------------------------------------------- #
add_subdirectory(block_solver)
add_subdirectory(investment_solver)
add_subdirectory(sddp_solver)
add_subdirectory(tssb_solver)
add_subdirectory(ucblock_solver)

add_subdirectory(chgcfg)

# --------------------------------------------------------------------------- #
