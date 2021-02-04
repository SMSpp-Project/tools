# ----- Requirements -------------------------------------------------------- #
find_package(UCBlock)
find_package(SDDPBlock)
find_package(StochasticBlock)
find_package(MILPSolver)

# ----- sddp_solver --------------------------------------------------------- #
if (UCBlock_FOUND AND
    SDDPBlock_FOUND AND
    StochasticBlock_FOUND AND
    MILPSolver_FOUND)

    add_executable(sddp_solver sddp_solver.cpp CutProcessing.cpp)
    target_compile_features(sddp_solver PUBLIC cxx_std_17)
    target_include_directories(sddp_solver PRIVATE
                               ../ucblock_solver)
    target_link_libraries(sddp_solver PRIVATE
                          SMS++::UCBlock
                          SMS++::SDDPBlock
                          SMS++::StochasticBlock
                          SMS++::MILPSolver)

    # Install
    install(TARGETS sddp_solver
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # Generate and install shell completion scripts

    configure_file(../cmake/completion.bash.in sddp_solver-completion.bash)
    configure_file(../cmake/completion.zsh.in sddp_solver-completion.zsh)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sddp_solver-completion.bash
            DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
            RENAME sddp_solver)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sddp_solver-completion.zsh
            DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
            RENAME "_sddp_solver")
endif ()

# --------------------------------------------------------------------------- #
