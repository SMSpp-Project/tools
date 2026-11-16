# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(UCBlock)
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

# ----- ucblock_solver ------------------------------------------------------ #
if (TARGET SMS++::UCBlock)

    add_executable(ucblock_solver ../common_utils.cpp
                                  ucblock_solver.cpp)
    target_include_directories(ucblock_solver PRIVATE ..)
    target_link_libraries(ucblock_solver PRIVATE SMS++::UCBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(ucblock_solver PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(ucblock_solver PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(ucblock_solver PRIVATE SMS++::MILPSolver)
    endif ()

    # Install
    smspp_install(ucblock_solver)
    smspp_install_data(ucblock_solver "solve a Unit Commitment problem"
            examples/Bus_Test.nc4)

endif ()

# --------------------------------------------------------------------------- #
