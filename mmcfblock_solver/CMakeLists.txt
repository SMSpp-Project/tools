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

# ----- Settings ------------------------------------------------------------ #
# The classes of the modules register themselves in the factories from
# static objects, which nothing here names: a shared library is kept by
# what its module asks of the linker, while an archive has to be taken
# whole [see SMSppModule.cmake of SMS++].
smspp_keep_modules()

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
