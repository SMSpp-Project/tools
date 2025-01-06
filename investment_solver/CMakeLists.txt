# ----- Settings ------------------------------------------------------------ #
# Since we are using the block factory, objects from linked libraries
# may wrongly appear unused, and by default the linker does not include them,
# so we have to force the linking.
if (BUILD_SHARED_LIBS)
    if (UNIX AND (NOT APPLE))
        add_link_options("-Wl,--no-as-needed")
    endif ()
else ()
    if (MSVC)
        add_link_options("/WHOLEARCHIVE")
        add_link_options("/FORCE:MULTIPLE")
    else () # Unix
        if (APPLE)
            add_link_options("-Wl,-all_load")
        else ()
            add_link_options("-Wl,--whole-archive,--allow-multiple-definition")
        endif ()
    endif ()
endif ()

# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(UCBlock)
    find_package(SDDPBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# ----- investment_solver --------------------------------------------------- #
if (TARGET SMS++::UCBlock AND
    TARGET SMS++::SDDPBlock)

    add_executable(investment_solver
                   investment_solver.cpp ../sddp_solver/CutProcessing.cpp
                   InvestmentFunction.cpp InvestmentBlock.cpp)
    target_compile_features(investment_solver PUBLIC cxx_std_17)
    target_include_directories(investment_solver PRIVATE ../ucblock_solver)
    target_include_directories(investment_solver PRIVATE ../sddp_solver)
    target_link_libraries(investment_solver PRIVATE
                          SMS++::UCBlock
                          SMS++::SDDPBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(investment_solver PRIVATE
                              SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(investment_solver PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(investment_solver PRIVATE SMS++::MILPSolver)
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
