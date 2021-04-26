# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    find_package(UCBlock)
    find_package(DPSolver)
    find_package(MILPSolver)
endif ()

# ----- thermalunit_solver -------------------------------------------------- #
if (TARGET UCBlock)
    add_executable(thermalunit_solver thermalunit_solver.cpp)
    target_include_directories(thermalunit_solver PRIVATE ../block_solver)
    target_link_libraries(thermalunit_solver PRIVATE SMS++::UCBlock)

    if (TARGET DPSolver)
        target_link_libraries(thermalunit_solver PRIVATE SMS++::DPSolver)
    endif ()
    if (TARGET MILPSolver)
        target_link_libraries(thermalunit_solver PRIVATE SMS++::MILPSolver)
    endif ()

    # Install
    install(TARGETS thermalunit_solver
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # Generate and install shell completion scripts
    configure_file(../cmake/completion.bash.in thermalunit_solver-completion.bash)
    configure_file(../cmake/completion.zsh.in thermalunit_solver-completion.zsh)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/thermalunit_solver-completion.bash
            DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
            RENAME thermalunit_solver)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/thermalunit_solver-completion.zsh
            DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
            RENAME "_thermalunit_solver")
endif ()

# --------------------------------------------------------------------------- #
