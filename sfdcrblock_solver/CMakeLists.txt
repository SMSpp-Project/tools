# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SingleFlowDCRBlock)
    # Solvers
    find_package(MILPSolver)
endif ()

# ----- Settings ------------------------------------------------------------ #
# The classes of the modules register themselves in the factories from
# static objects, which nothing here names: a shared library is kept by
# what its module asks of the linker, while an archive has to be taken
# whole [see SMSppModule.cmake of SMS++].
smspp_keep_modules()

# ----- sfdcrblock_solver --------------------------------------------------- #
if (TARGET SMS++::SingleFlowDCRBlock)

    add_executable(sfdcrblock_solver ../common_utils.cpp
                   sfdcrblock_solver.cpp)
    target_include_directories(sfdcrblock_solver PRIVATE ..)
    target_link_libraries(sfdcrblock_solver PRIVATE
            SMS++::SingleFlowDCRBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(sfdcrblock_solver PRIVATE
                SMS++::MILPSolver)
    endif ()

    # Install
    smspp_install(sfdcrblock_solver)
    smspp_install_data(sfdcrblock_solver
            "solve a Single-flow Delay-Constrained Routing problem")

endif ()

# --------------------------------------------------------------------------- #
