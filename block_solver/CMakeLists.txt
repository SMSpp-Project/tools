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
    find_package(SMS++)
    find_package(MCFBlock)
    find_package(InvestmentBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
    find_package(LEMONSolver)
endif ()

# ----- block_solver -------------------------------------------------------- #
add_executable(block_solver ../block_solver/common_utils.cpp
                            block_solver.cpp)
target_link_libraries(block_solver PRIVATE SMS++::SMS++)

if (WIN32 AND BUILD_SHARED_LIBS AND NOT DEFINED ENV{CONDA_BUILD})
    # copy runtime DLLs next to the exe after build
    add_custom_command(TARGET block_solver POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       $<TARGET_RUNTIME_DLLS:block_solver>
                       $<TARGET_FILE_DIR:block_solver>
                       COMMAND_EXPAND_LISTS
                       COMMENT "Copying runtime DLLs next to block_solver.exe")
endif()

if (TARGET SMS++::MCFBlock)
    target_link_libraries(block_solver PRIVATE SMS++::MCFBlock)
endif ()

if (TARGET SMS++::InvestmentBlock)
    target_link_libraries(block_solver PRIVATE SMS++::InvestmentBlock)
endif ()

if (TARGET SMS++::LagrangianDualSolver)
    target_link_libraries(block_solver PRIVATE SMS++::LagrangianDualSolver)
endif ()

if (TARGET SMS++::BundleSolver)
    target_link_libraries(block_solver PRIVATE SMS++::BundleSolver)
endif ()

if (TARGET SMS++::MILPSolver)
    target_link_libraries(block_solver PRIVATE SMS++::MILPSolver)
endif ()

if (TARGET SMS++::LEMONSolver)
    target_link_libraries(block_solver PRIVATE SMS++::LEMONSolver)
endif ()

install(TARGETS block_solver
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

configure_file(../cmake/completion.bash.in block_solver-completion.bash)
configure_file(../cmake/completion.zsh.in block_solver-completion.zsh)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/block_solver-completion.bash
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME block_solver)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/block_solver-completion.zsh
        DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
        RENAME "_block_solver")

# --------------------------------------------------------------------------- #
