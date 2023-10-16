# ----- Settings ------------------------------------------------------------ #
# An option is an ON/OFF user-settable cache variable.
option(tools_USE_DL "Use dynamic loading" ON)

# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    find_package(SMS++)
    find_package(UCBlock)
    find_package(MILPSolver)
endif ()

# ----- block_solver -------------------------------------------------------- #
add_executable(block_solver block_solver.cpp)
target_compile_features(block_solver PUBLIC cxx_std_17)

if (BUILD_SHARED_LIBS AND tools_USE_DL)
    # If shared libraries have been built and the option is ON,
    # block_solver will be using dynamic loading.
    # It only needs SMS++ core library.
    if (TARGET SMS++::SMS++)
        # Runtime path with the build tree directories
        # containing the libraries.
        set(CMAKE_BUILD_RPATH ${SMSPP_BUILD_RPATH})

        # Enable DL in the code.
        target_compile_definitions(block_solver PRIVATE USE_DL)
        target_link_libraries(block_solver PRIVATE SMS++::SMS++ dl)
    endif ()
else ()
    # Otherwise, link all the supported libraries.
    if (TARGET SMS++::UCBlock AND
        TARGET SMS++::MILPSolver)
        target_link_libraries(block_solver PRIVATE
                              SMS++::UCBlock
                              SMS++::MILPSolver)
    endif ()
endif ()

# ----- Install ------------------------------------------------------------- #
install(TARGETS block_solver
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ----- Generate and install shell completion scripts ----------------------- #
configure_file(../cmake/completion.bash.in block_solver-completion.bash)
configure_file(../cmake/completion.zsh.in block_solver-completion.zsh)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/block_solver-completion.bash
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME block_solver)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/block_solver-completion.zsh
        DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
        RENAME "_block_solver")

# --------------------------------------------------------------------------- #
