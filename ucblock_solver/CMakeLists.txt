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
    find_package(UCBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# ----- ucblock_solver ------------------------------------------------------ #
if (TARGET SMS++::UCBlock)

    add_executable(ucblock_solver ../common_utils.cpp
                                  ucblock_solver.cpp)
    target_include_directories(ucblock_solver PRIVATE ..)
    target_link_libraries(ucblock_solver PRIVATE SMS++::UCBlock)

    # Optional: dump every inner ThermalUnitBlock at every Lagrangian iteration
    # (as TUB-<unit>-<iteration>.nc4) while solving under a LagrangianDualSolver,
    # for the run-lagrangian-timing study. Off by default; enable with
    #   cmake -DUCBLOCK_SOLVER_SAVE_TUB=ON ...
    option(UCBLOCK_SOLVER_SAVE_TUB
           "ucblock_solver: dump per-iteration ThermalUnitBlocks (SAVE_TUB)" OFF)
    if (UCBLOCK_SOLVER_SAVE_TUB)
        target_compile_definitions(ucblock_solver PRIVATE SAVE_TUB=1)
    endif ()

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(ucblock_solver PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(ucblock_solver PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(ucblock_solver PRIVATE SMS++::MILPSolver)
    endif ()

    # Install
    smspp_install(ucblock_solver)

    # timing_harness: small stand-alone benchmark for the run-lagrangian-timing
    # study; it times the two DP solvers (ThermalUnit[Ext]DPSolver, in UCBlock)
    # and, when available, a :MILPSolver on the T+P/C formulation (MILPSolver).
    add_executable(timing_harness timing_harness.cpp)
    target_link_libraries(timing_harness PRIVATE SMS++::UCBlock)
    if (TARGET SMS++::MILPSolver)
        target_link_libraries(timing_harness PRIVATE SMS++::MILPSolver)
    endif ()
    smspp_install(timing_harness)

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
