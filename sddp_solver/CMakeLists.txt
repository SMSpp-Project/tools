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
    find_package(SDDPBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# ----- sddp_solver --------------------------------------------------------- #
if (TARGET SMS++::UCBlock AND
    TARGET SMS++::SDDPBlock)

    add_executable(sddp_solver ../common_utils.cpp sddp_solver.cpp
                   SDDPBlockSolutionOutput.h UCBlockSolutionOutput.h)
    target_compile_features(sddp_solver PUBLIC cxx_std_17)
    target_include_directories(sddp_solver PRIVATE ..)
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

    # Install
    smspp_install(sddp_solver)
    smspp_install_data(sddp_solver
            "solve a multi-stage stochastic problem by SDDP"
            examples/SDDPBlock.nc4 examples/Block_0.nc4
            examples/Block_1.nc4 examples/Block_2.nc4)

endif ()

# --------------------------------------------------------------------------- #
