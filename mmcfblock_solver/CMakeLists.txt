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
    find_package(MMCFBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# ----- mmcfblock_solver ---------------------------------------------------- #
if (TARGET SMS++::MMCFBlock)

    add_executable(mmcfblock_solver ../common_utils.cpp
                   mmcfblock_solver.cpp)
    target_include_directories(mmcfblock_solver PRIVATE ..)
    target_link_libraries(mmcfblock_solver PRIVATE
            SMS++::MMCFBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(mmcfblock_solver PRIVATE
                SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(mmcfblock_solver PRIVATE
                SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(mmcfblock_solver PRIVATE
                SMS++::MILPSolver)
    endif ()

    # Install
    smspp_install(mmcfblock_solver)
    smspp_install_data(mmcfblock_solver
            "solve a Multicommodity Min-Cost Flow problem"
            examples/small.std)

endif ()

# --------------------------------------------------------------------------- #
