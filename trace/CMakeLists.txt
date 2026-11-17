# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    find_package(SATBlock)
    find_package(MILPSolver)
    find_package(BranchAndXSolver)
endif ()

# ----- Settings ------------------------------------------------------------ #
# The Solver are made by the factory out of the BlockSolverConfig, hence the
# modules are taken whole [see SMSppModule.cmake of SMS++].
smspp_keep_modules()

# ----- trace --------------------------------------------------------------- #
if (TARGET SMS++::SATBlock AND TARGET SMS++::MILPSolver
    AND TARGET SMS++::BranchAndXSolver)

    add_executable(trace ../common_utils.cpp trace.cpp)
    target_compile_features(trace PUBLIC cxx_std_20)
    target_include_directories(trace PRIVATE ..)
    target_link_libraries(trace PRIVATE SMS++::SATBlock
                                        SMS++::MILPSolver
                                        SMS++::BranchAndXSolver)

    smspp_install(trace)
    smspp_install_data(trace "traces of the search of the Solver")

endif ()

# --------------------------------------------------------------------------- #
