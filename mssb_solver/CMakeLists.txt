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
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# ----- Settings ------------------------------------------------------------ #
# The classes of the modules register themselves in the factories from
# static objects, which nothing here names: a shared library is kept by
# what its module asks of the linker, while an archive has to be taken
# whole [see SMSppModule.cmake of SMS++].
smspp_keep_modules()

# ----- mssb_solver --------------------------------------------------------- #
if (TARGET SMS++::UCBlock AND
    TARGET SMS++::MultiStageStochasticBlock)

    add_executable(mssb_solver ../common_utils.cpp
                               mssb_solver.cpp)
    target_compile_features(mssb_solver PUBLIC cxx_std_20)
    target_include_directories(mssb_solver PRIVATE .. ../ucblock_solver)
    target_link_libraries(mssb_solver PRIVATE
                          SMS++::UCBlock
                          SMS++::TwoStageStochasticBlock
                          SMS++::MultiStageStochasticBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(mssb_solver PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(mssb_solver PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(mssb_solver PRIVATE SMS++::MILPSolver)
    endif ()

    # Install
    smspp_install(mssb_solver)
    smspp_install_data(mssb_solver "solve a multi-stage stochastic problem")

endif ()

# --------------------------------------------------------------------------- #
