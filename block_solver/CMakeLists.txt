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
    find_package(SMS++)
    find_package(MCFBlock)
    find_package(InvestmentBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
    find_package(MCFClassSolver)
    find_package(MCFLemonSolver)
endif ()

# ----- block_solver -------------------------------------------------------- #
add_executable(block_solver ../common_utils.cpp
                            block_solver.cpp)
target_include_directories(block_solver PRIVATE ..)
target_link_libraries(block_solver PRIVATE SMS++::SMS++)

if (TARGET SMS++::MCFBlock)
    target_link_libraries(block_solver PRIVATE SMS++::MCFBlock)
endif ()

if (TARGET SMS++::InvestmentBlock)
    target_link_libraries(block_solver PRIVATE SMS++::InvestmentBlock)
endif ()

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
