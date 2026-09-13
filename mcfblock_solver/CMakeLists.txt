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
    find_package(MCFBlock)
    # Solvers
    find_package(MCFClassSolver)
    find_package(MCFLemonSolver)
    find_package(MILPSolver)
endif ()

# ----- mcfblock_solver ----------------------------------------------------- #
if (TARGET SMS++::MCFBlock)

    add_executable(mcfblock_solver ../common_utils.cpp
                   mcfblock_solver.cpp)
    target_include_directories(mcfblock_solver PRIVATE ..)
    target_link_libraries(mcfblock_solver PRIVATE
            SMS++::MCFBlock)

    if (TARGET SMS++::MCFClassSolver)
        target_link_libraries(mcfblock_solver PRIVATE
                SMS++::MCFClassSolver)
    endif ()

    if (TARGET SMS++::MCFLemonSolver)
        target_link_libraries(mcfblock_solver PRIVATE
                SMS++::MCFLemonSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(mcfblock_solver PRIVATE
                SMS++::MILPSolver)
    endif ()

    # Install
    smspp_install(mcfblock_solver)
    smspp_install_data(mcfblock_solver
            "solve a Min-Cost Flow problem"
            examples/example.dmx)

endif ()

# --------------------------------------------------------------------------- #
