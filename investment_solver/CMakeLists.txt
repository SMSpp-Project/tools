# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(InvestmentBlock)
    # Solvers
    find_package(MILPSolver)
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
endif ()

# ----- investment_solver --------------------------------------------------- #
if (TARGET SMS++::InvestmentBlock)

    add_executable(investment_solver
                   investment_solver.cpp ../sddp_solver/CutProcessing.cpp
                   InvestmentFunction.cpp InvestmentBlock.cpp)
    target_compile_features(investment_solver PUBLIC cxx_std_17)
    target_include_directories(investment_solver PRIVATE ../ucblock_solver)
    target_include_directories(investment_solver PRIVATE ../sddp_solver)
    target_link_libraries(investment_solver PRIVATE SMS++::InvestmentBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(investment_solver PRIVATE SMS++::MILPSolver)
    endif ()

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(investment_solver PRIVATE
                SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(investment_solver PRIVATE SMS++::BundleSolver)
    endif ()

    # Install
    install(TARGETS investment_solver
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # Generate and install shell completion scripts
    configure_file(../cmake/completion.bash.in investment_solver-completion.bash)
    configure_file(../cmake/completion.zsh.in investment_solver-completion.zsh)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/investment_solver-completion.bash
            DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
            RENAME investment_solver)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/investment_solver-completion.zsh
            DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
            RENAME "_investment_solver")
endif ()

# --------------------------------------------------------------------------- #
