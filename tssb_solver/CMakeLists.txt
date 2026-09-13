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
    find_package(TwoStageStochasticBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# ----- tssb_solver --------------------------------------------------------- #
if (TARGET SMS++::UCBlock AND
    TARGET SMS++::TwoStageStochasticBlock)

    add_executable(tssb_solver ../common_utils.cpp
                               tssb_solver.cpp)
    target_compile_features(tssb_solver PUBLIC cxx_std_17)
    target_include_directories(tssb_solver PRIVATE .. ../ucblock_solver)
    target_link_libraries(tssb_solver PRIVATE
                          SMS++::UCBlock
                          SMS++::TwoStageStochasticBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(tssb_solver PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(tssb_solver PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(tssb_solver PRIVATE SMS++::MILPSolver)
    endif ()

    # Install
    smspp_install(tssb_solver)
    smspp_install_data(tssb_solver "solve a two-stage stochastic problem"
            examples/toy_tssb.nc4)

endif ()

# --------------------------------------------------------------------------- #
