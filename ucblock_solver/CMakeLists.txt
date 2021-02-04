# ----- Requirements -------------------------------------------------------- #
find_package(UCBlock)
find_package(MILPSolver)

# ----- ucblock_solver ------------------------------------------------------ #
if (UCBlock_FOUND AND MILPSolver_FOUND)

    add_executable(ucblock_solver ucblock_solver.cpp)
    target_include_directories(ucblock_solver PRIVATE
                               ../block_solver)
    target_link_libraries(ucblock_solver PRIVATE
                          SMS++::UCBlock
                          SMS++::MILPSolver)

    # Install
    install(TARGETS ucblock_solver
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # Generate and install shell completion scripts
    configure_file(../scripts/completion.bash.in ucblock_solver-completion.bash)
    configure_file(../scripts/completion.zsh.in ucblock_solver-completion.zsh)

    install(FILES ${PROJECT_BINARY_DIR}/ucblock_solver-completion.bash
            DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
            RENAME ucblock_solver)
    install(FILES ${PROJECT_BINARY_DIR}/ucblock_solver-completion.zsh
            DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
            RENAME "_ucblock_solver")
endif ()

# --------------------------------------------------------------------------- #
