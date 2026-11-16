# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(UCBlock)
    find_package(TwoStageStochasticBlock)
    find_package(MultiStageStochasticBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BendersDecompositionSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# ----- Settings ------------------------------------------------------------ #
# The classes of the modules register themselves in the factories from
# static objects, which nothing here names: a shared library is kept by
# what its module asks of the linker, while an archive has to be taken
# whole [see SMSppModule.cmake of SMS++].
smspp_keep_modules()

# ----- tssb_solver --------------------------------------------------------- #
if (TARGET SMS++::UCBlock AND
    TARGET SMS++::TwoStageStochasticBlock)

    add_executable(tssb_solver ../common_utils.cpp
                               tssb_solver.cpp)
    target_compile_features(tssb_solver PUBLIC cxx_std_20)
    target_include_directories(tssb_solver PRIVATE .. ../ucblock_solver)
    target_link_libraries(tssb_solver PRIVATE
                          SMS++::UCBlock
                          SMS++::TwoStageStochasticBlock)

    if (TARGET SMS++::MultiStageStochasticBlock)
        target_link_libraries(tssb_solver PRIVATE
                              SMS++::MultiStageStochasticBlock)
    endif ()

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(tssb_solver PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(tssb_solver PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::BendersDecompositionSolver)
        target_link_libraries(tssb_solver PRIVATE
                              SMS++::BendersDecompositionSolver)
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
