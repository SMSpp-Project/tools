# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SMS++)
    find_package(MCFBlock)
    find_package(MMCFBlock)
    find_package(InvestmentBlock)
    find_package(UCBlock)
    find_package(SDDPBlock)
    find_package(TwoStageStochasticBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
    find_package(MCFClassSolver)
    find_package(MCFLemonSolver)
endif ()

# ----- Settings ------------------------------------------------------------ #
# The classes of the modules register themselves in the factories from
# static objects, which nothing here names: a shared library is kept by
# what its module asks of the linker, while an archive has to be taken
# whole [see SMSppModule.cmake of SMS++].
smspp_keep_modules()

# ----- block_solver -------------------------------------------------------- #
add_executable(block_solver ../common_utils.cpp
                            block_solver.cpp)
target_include_directories(block_solver PRIVATE ..)
target_link_libraries(block_solver PRIVATE SMS++::SMS++)

if (TARGET SMS++::MCFBlock)
    target_link_libraries(block_solver PRIVATE SMS++::MCFBlock)
endif ()
if (TARGET SMS++::MMCFBlock)
    target_link_libraries(block_solver PRIVATE SMS++::MMCFBlock)
endif ()

# each Block whose module is there, which block_solver reads by the factory
foreach (block InvestmentBlock UCBlock SDDPBlock TwoStageStochasticBlock)
    if (TARGET SMS++::${block})
        target_link_libraries(block_solver PRIVATE SMS++::${block})
    endif ()
endforeach ()

if (TARGET SMS++::LagrangianDualSolver)
    target_link_libraries(block_solver PRIVATE SMS++::LagrangianDualSolver)
endif ()

if (TARGET SMS++::BundleSolver)
    target_link_libraries(block_solver PRIVATE SMS++::BundleSolver)
endif ()

if (TARGET SMS++::MILPSolver)
    target_link_libraries(block_solver PRIVATE SMS++::MILPSolver)
endif ()

if (TARGET SMS++::MCFClassSolver)
    target_link_libraries(block_solver PRIVATE SMS++::MCFClassSolver)
endif ()

if (TARGET SMS++::MCFLemonSolver)
    target_link_libraries(block_solver PRIVATE SMS++::MCFLemonSolver)
endif ()

# Install
smspp_install(block_solver)
smspp_install_data(block_solver "solve any SMS++ Block")

# --------------------------------------------------------------------------- #
