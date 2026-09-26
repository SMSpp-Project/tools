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
    find_package(InvestmentBlock)
    # the inner Block of an InvestmentBlock may be a stochastic one: the
    # TwoStageStochasticBlock comes with InvestmentBlock, the multi-stage one
    # has to be asked for, since nothing depends on it, the Block being
    # reached polymorphically
    find_package(MultiStageStochasticBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::InvestmentBlock)

    add_executable(investmentblock_solver ../common_utils.cpp
                                          investmentblock_solver.cpp)
    target_include_directories(investmentblock_solver PRIVATE ..)
    target_link_libraries(investmentblock_solver PRIVATE SMS++::InvestmentBlock)

    if (TARGET SMS++::MultiStageStochasticBlock)
        target_link_libraries(investmentblock_solver PRIVATE
                              SMS++::MultiStageStochasticBlock)
    endif ()

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(investmentblock_solver PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(investmentblock_solver PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(investmentblock_solver PRIVATE SMS++::MILPSolver)
    endif ()

    # Install
    smspp_install(investmentblock_solver)
    smspp_install_data(investmentblock_solver "solve an investment problem"
            examples/InvestmentBlockBus.nc4)

endif ()

# --------------------------------------------------------------------------- #
