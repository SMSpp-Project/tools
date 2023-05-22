# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    find_package(UCBlock)
    find_package(MILPSolver)
    find_package(BundleSolver)
    find_package(LagrangianDualSolver)
endif ()

# ----- ucblock_solver ------------------------------------------------------ #
if (TARGET SMS++::UCBlock)
    add_executable(ucblock_solver ucblock_solver.cpp)
    target_include_directories(ucblock_solver PRIVATE ../block_solver)
    target_link_libraries(ucblock_solver PRIVATE SMS++::UCBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(ucblock_solver PRIVATE SMS++::MILPSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(ucblock_solver PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(ucblock_solver PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (UNIX AND (NOT APPLE))
        target_link_libraries(ucblock_solver PRIVATE stdc++fs)
    endif ()

    # Install
    install(TARGETS ucblock_solver
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # Generate and install shell completion scripts
    configure_file(../cmake/completion.bash.in ucblock_solver-completion.bash)
    configure_file(../cmake/completion.zsh.in ucblock_solver-completion.zsh)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ucblock_solver-completion.bash
            DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
            RENAME ucblock_solver)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ucblock_solver-completion.zsh
            DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
            RENAME "_ucblock_solver")
endif ()

# --------------------------------------------------------------------------- #
