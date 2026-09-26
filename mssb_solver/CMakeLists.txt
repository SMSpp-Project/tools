# ----- Settings ------------------------------------------------------------ #
# The classes of the modules register themselves in the factories from static
# objects, which nothing in the tool names: a shared library is kept by what
# the module itself asks of the linker [see its CMakeLists.txt], while an
# archive has to be taken whole, or those objects are dropped.
if (NOT BUILD_SHARED_LIBS)
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
    find_package(MultiStageStochasticBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# ----- mssb_solver --------------------------------------------------------- #
if (TARGET SMS++::UCBlock AND
    TARGET SMS++::MultiStageStochasticBlock)

    add_executable(mssb_solver ../common_utils.cpp
                               mssb_solver.cpp)
    target_compile_features(mssb_solver PUBLIC cxx_std_17)
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
