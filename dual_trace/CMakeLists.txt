# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    find_package(BundleSolver)
    find_package(SubGradSolver)
    find_package(BDMinQuadSolver)
endif ()

# ----- dual_trace ---------------------------------------------------------- #
# BDMinQuadSolver solves the master problem of BundleSolver [see
# config/MasterBSCfg.txt]: linked so that it registers in the factory
if (TARGET SMS++::BundleSolver AND TARGET SMS++::SubGradSolver
    AND TARGET SMS++::BDMinQuadSolver)

    add_executable(dual_trace ../common_utils.cpp dual_trace.cpp)
    target_compile_features(dual_trace PUBLIC cxx_std_20)
    target_include_directories(dual_trace PRIVATE ..)
    target_link_libraries(dual_trace PRIVATE SMS++::BundleSolver
                                             SMS++::SubGradSolver
                                             SMS++::BDMinQuadSolver)

    smspp_install(dual_trace)
    smspp_install_data(dual_trace
                       "traces of the dual solvers for opt-playground")

endif ()

# --------------------------------------------------------------------------- #
