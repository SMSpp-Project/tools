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
    find_package(CapacitatedFacilityLocationBlock)
    # Solvers
    find_package(MILPSolver)
endif ()

# ----- cflblock_solver ----------------------------------------------------- #
if (TARGET SMS++::CapacitatedFacilityLocationBlock)

    add_executable(cflblock_solver ../common_utils.cpp
                   cflblock_solver.cpp)
    target_include_directories(cflblock_solver PRIVATE ..)
    target_link_libraries(cflblock_solver PRIVATE
            SMS++::CapacitatedFacilityLocationBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(cflblock_solver PRIVATE
                SMS++::MILPSolver)
    endif ()

    # Install
    smspp_install(cflblock_solver)
    smspp_install_data(cflblock_solver
            "solve a Capacitated Facility Location problem"
            examples/example.txt)

    # Generate and install shell completion scripts
    set(toolName cflblock_solver)
    configure_file(../cmake/completion.bash.in
            cflblock_solver-completion.bash)
    configure_file(../cmake/completion.zsh.in
            cflblock_solver-completion.zsh)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cflblock_solver-completion.bash
            DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
            RENAME cflblock_solver)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cflblock_solver-completion.zsh
            DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
            RENAME "_cflblock_solver")
endif ()

# --------------------------------------------------------------------------- #
