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
    find_package(SingleFlowDCRBlock)
    # Solvers
    find_package(MILPSolver)
endif ()

# ----- sfdcrblock_solver --------------------------------------------------- #
if (TARGET SMS++::SingleFlowDCRBlock)

    add_executable(sfdcrblock_solver ../common_utils.cpp
                   sfdcrblock_solver.cpp)
    target_include_directories(sfdcrblock_solver PRIVATE ..)
    target_link_libraries(sfdcrblock_solver PRIVATE
            SMS++::SingleFlowDCRBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(sfdcrblock_solver PRIVATE
                SMS++::MILPSolver)
    endif ()

    # Install
    smspp_install(sfdcrblock_solver)
    smspp_install_data(sfdcrblock_solver
            "solve a Single-flow Delay-Constrained Routing problem")

    # Generate and install shell completion scripts
    set(toolName sfdcrblock_solver)
    configure_file(../cmake/completion.bash.in
            sfdcrblock_solver-completion.bash)
    configure_file(../cmake/completion.zsh.in
            sfdcrblock_solver-completion.zsh)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sfdcrblock_solver-completion.bash
            DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
            RENAME sfdcrblock_solver)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sfdcrblock_solver-completion.zsh
            DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
            RENAME "_sfdcrblock_solver")
endif ()

# --------------------------------------------------------------------------- #
