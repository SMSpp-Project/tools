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
        add_link_options("/WHOLEARCHIVE" "/FORCE:MULTIPLE")
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
    find_package(InvestmentBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::InvestmentBlock)

    add_executable(investmentblock_solver ../common_utils.cpp
                                          investmentblock_solver.cpp)
    target_include_directories(investmentblock_solver PRIVATE ..)
    target_link_libraries(investmentblock_solver PRIVATE SMS++::InvestmentBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(investmentblock_solver PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(investmentblock_solver PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(investmentblock_solver PRIVATE SMS++::MILPSolver)
    endif ()

    # Install
    smspp_install(investmentblock_solver)

    # Generate and install shell completion scripts
    configure_file(../cmake/completion.bash.in investmentblock_solver-completion.bash)
    configure_file(../cmake/completion.zsh.in investmentblock_solver-completion.zsh)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/investmentblock_solver-completion.bash
            DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
            RENAME investmentblock_solver)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/investmentblock_solver-completion.zsh
            DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
            RENAME "_investmentblock_solver")
endif ()

# --------------------------------------------------------------------------- #
