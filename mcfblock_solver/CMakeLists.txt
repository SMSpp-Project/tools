# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(MCFBlock)
    # Solvers
    find_package(MCFClassSolver)
    find_package(MCFLemonSolver)
    find_package(MILPSolver)
endif ()

# ----- Settings ------------------------------------------------------------ #
# The classes of the modules register themselves in the factories from
# static objects, which nothing here names: a shared library is kept by
# what its module asks of the linker, while an archive has to be taken
# whole [see SMSppModule.cmake of SMS++].
smspp_keep_modules()

# ----- mcfblock_solver ----------------------------------------------------- #
if (TARGET SMS++::MCFBlock)

    add_executable(mcfblock_solver ../common_utils.cpp
                   mcfblock_solver.cpp)
    target_include_directories(mcfblock_solver PRIVATE ..)
    target_link_libraries(mcfblock_solver PRIVATE
            SMS++::MCFBlock)

    if (TARGET SMS++::MCFClassSolver)
        target_link_libraries(mcfblock_solver PRIVATE
                SMS++::MCFClassSolver)
    endif ()

    if (TARGET SMS++::MCFLemonSolver)
        target_link_libraries(mcfblock_solver PRIVATE
                SMS++::MCFLemonSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(mcfblock_solver PRIVATE
                SMS++::MILPSolver)
    endif ()

    # Install
    smspp_install(mcfblock_solver)
    smspp_install_data(mcfblock_solver
            "solve a Min-Cost Flow problem"
            examples/example.dmx)

endif ()

# --------------------------------------------------------------------------- #
