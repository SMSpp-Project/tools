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

# ----- Requirements -------------------------------------------------------- #
find_package(UCBlock)
find_package(MILPSolver)
find_package(SMILPBlock)
find_package(SDDPBlock)

# ----- block_solver -------------------------------------------------------- #
add_executable(block_solver block_solver.cpp)
target_compile_features(block_solver PUBLIC cxx_std_17)

if (BUILD_SHARED_LIBS AND tools_USE_DL)
    # If shared libraries have been built and the option is ON,
    # block_solver will be using dynamic loading.
    # It only needs SMS++ core library.
    find_package(SMSpp)
    if (SMSpp_FOUND)

        # Runtime path with the build tree directories
        # containing the libraries.
        set(CMAKE_BUILD_RPATH ${SMSPP_BUILD_RPATH})

        # Enable DL in the code.
        target_compile_definitions(block_solver PRIVATE USE_DL)

        target_link_libraries(block_solver PRIVATE SMS++::SMSpp dl)
    endif ()
else ()
    # Otherwise, link all the supported libraries.
    if (UCBlock_FOUND AND MILPSolver_FOUND)
        target_link_libraries(block_solver PRIVATE
                              SMS++::MILPSolver
                              SMS++::UCBlock)
    endif ()
endif ()

install(TARGETS block_solver
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ----- sddp_greedy_solver -------------------------------------------------- #
if (SDDPBlock_FOUND AND MILPSolver_FOUND)
    add_executable(sddp_greedy_solver sddp_greedy_solver.cpp)
    target_compile_features(sddp_greedy_solver PUBLIC cxx_std_17)

    target_link_libraries(sddp_greedy_solver PRIVATE
                          SMS++::UCBlock
                          SMS++::SDDPBlock
                          SMS++::StochasticBlock
                          SMS++::MILPSolver)
endif ()

# ----- ucblock/thermalunit_solver ------------------------------------------ #
if (UCBlock_FOUND AND MILPSolver_FOUND)
    add_executable(thermalunit_solver thermalunit_solver.cpp)
    target_link_libraries(thermalunit_solver PRIVATE
                          SMS++::UCBlock
                          SMS++::MILPSolver)

    add_executable(ucblock_solver ucblock_solver.cpp)
    target_link_libraries(ucblock_solver PRIVATE
                          SMS++::UCBlock
                          SMS++::MILPSolver)

    install(TARGETS thermalunit_solver ucblock_solver
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()

# ----- milp_solver --------------------------------------------------------- #
if (SMILPBlock_FOUND AND MILPSolver_FOUND)
    add_executable(milp_solver milp_solver.cpp)
    target_link_libraries(milp_solver PRIVATE
                          SMS++::SMILPBlock
                          SMS++::MILPSolver)

    install(TARGETS milp_solver
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()

# ----- Generate and install shell completion scripts ----------------------- #
get_directory_property(toolNames BUILDSYSTEM_TARGETS)
foreach (toolName ${toolNames})
    configure_file(scripts/completion.bash.in ${toolName}-completion.bash)
    configure_file(scripts/completion.zsh.in ${toolName}-completion.zsh)

    install(FILES ${PROJECT_BINARY_DIR}/${toolName}-completion.bash
            DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
            RENAME ${toolName})
    install(FILES ${PROJECT_BINARY_DIR}/${toolName}-completion.zsh
            DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
            RENAME "_${toolName}")
endforeach ()

# --------------------------------------------------------------------------- #
# Just consuming these to avoid warnings
if (BUILD_TESTING)
endif ()

if (CMAKE_EXPORT_PACKAGE_REGISTRY)
endif ()

# --------------------------------------------------------------------------- #
