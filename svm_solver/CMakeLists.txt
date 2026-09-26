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
    find_package(SVMBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# ----- svm_solver ---------------------------------------------------------- #
if (TARGET SMS++::SVMBlock)

    add_executable(svm_solver ../common_utils.cpp
                              ../ml_utils.cpp
                              svm_solver.cpp)
    target_compile_features(svm_solver PUBLIC cxx_std_17)
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
