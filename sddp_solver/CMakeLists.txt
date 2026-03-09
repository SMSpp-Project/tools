# ----- Settings ------------------------------------------------------------ #
# Since we are using the block factory, objects from linked libraries
# may wrongly appear unused, and by default the linker does not include them,
# so we have to force the linking.
if (BUILD_SHARED_LIBS)
    if (UNIX)
        if (APPLE)
            add_link_options("-Wl,-dead_strip_dylibs")
        else ()
            add_link_options("-Wl,--no-as-needed")
        endif ()
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
    find_package(SDDPBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# ----- sddp_solver --------------------------------------------------------- #
if (TARGET SMS++::UCBlock AND
    TARGET SMS++::SDDPBlock)

    add_executable(sddp_solver sddp_solver.cpp CutProcessing.cpp)
    target_compile_features(sddp_solver PUBLIC cxx_std_17)
    target_include_directories(sddp_solver PRIVATE ../block_solver ../ucblock_solver)
    target_link_libraries(sddp_solver PRIVATE
                          SMS++::UCBlock
                          SMS++::SDDPBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(sddp_solver PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(sddp_solver PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(sddp_solver PRIVATE SMS++::MILPSolver)
    endif ()

    if (WIN32 AND BUILD_SHARED_LIBS)
        # copy runtime DLLs next to the exe after build
        add_custom_command(TARGET sddp_solver POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:sddp_solver>
                $<TARGET_FILE_DIR:sddp_solver>
                COMMAND_EXPAND_LISTS
                COMMENT "Copying runtime DLLs next to sddp_solver.exe"
        )
    endif()

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
