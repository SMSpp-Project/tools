# ----- Requirements -------------------------------------------------------- #
find_package(UCBlock)
find_package(MILPSolver)

# ----- thermalunit_solver -------------------------------------------------- #
if (UCBlock_FOUND AND MILPSolver_FOUND)

    add_executable(thermalunit_solver thermalunit_solver.cpp)
    target_include_directories(thermalunit_solver PRIVATE
                               ../block_solver)
    target_link_libraries(thermalunit_solver PRIVATE
                          SMS++::UCBlock
                          SMS++::MILPSolver)

    find_package(DPSolver)
    if (DPSolver_FOUND)
        target_link_libraries(thermalunit_solver PRIVATE SMS++::DPSolver)
    endif ()

    # Install
    install(TARGETS thermalunit_solver
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # Generate and install shell completion scripts
    configure_file(../scripts/completion.bash.in thermalunit_solver-completion.bash)
    configure_file(../scripts/completion.zsh.in thermalunit_solver-completion.zsh)

    install(FILES ${PROJECT_BINARY_DIR}/thermalunit_solver-completion.bash
            DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
            RENAME thermalunit_solver)
    install(FILES ${PROJECT_BINARY_DIR}/thermalunit_solver-completion.zsh
            DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
            RENAME "_thermalunit_solver")
endif ()

# --------------------------------------------------------------------------- #
