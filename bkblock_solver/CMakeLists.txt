# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(BinaryKnapsackBlock)
    # Solvers
    find_package(MILPSolver)
endif ()

# ----- Settings ------------------------------------------------------------ #
# The classes of the modules register themselves in the factories from
# static objects, which nothing here names: a shared library is kept by
# what its module asks of the linker, while an archive has to be taken
# whole [see SMSppModule.cmake of SMS++].
smspp_keep_modules()

# ----- bkblock_solver ------------------------------------------------------ #
if (TARGET SMS++::BinaryKnapsackBlock)

    add_executable(bkblock_solver ../common_utils.cpp
                   bkblock_solver.cpp)
    target_include_directories(bkblock_solver PRIVATE ..)
    target_link_libraries(bkblock_solver PRIVATE
            SMS++::BinaryKnapsackBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(bkblock_solver PRIVATE
                SMS++::MILPSolver)
    endif ()

    # Install
    smspp_install(bkblock_solver)
    smspp_install_data(bkblock_solver
            "solve a Binary Knapsack problem"
            examples/example.txt)

endif ()

# --------------------------------------------------------------------------- #
