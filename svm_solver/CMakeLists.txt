# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SVMBlock)
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

# ----- svm_solver ---------------------------------------------------------- #
if (TARGET SMS++::SVMBlock)

    add_executable(svm_solver ../common_utils.cpp
                              ../ml_utils.cpp
                              svm_solver.cpp)
    target_compile_features(svm_solver PUBLIC cxx_std_20)
    target_include_directories(svm_solver PRIVATE ..)
    target_link_libraries(svm_solver PRIVATE SMS++::SVMBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(svm_solver PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(svm_solver PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(svm_solver PRIVATE SMS++::MILPSolver)
    endif ()

    # Install
    smspp_install(svm_solver)
    smspp_install_data(svm_solver "train a Support Vector Machine")

endif ()

# --------------------------------------------------------------------------- #
