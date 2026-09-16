# ----- Settings ------------------------------------------------------------ #
# Since we are using the block factory, objects from linked libraries
# may wrongly appear unused, and by default the linker does not include them,
# so we have to force the linking.
if (BUILD_SHARED_LIBS)
    if (UNIX AND (NOT APPLE))
        add_link_options("-Wl,--no-as-needed")
    endif ()
else ()
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
    find_package(BinaryKnapsackBlock)
    # Solvers
    find_package(MILPSolver)
endif ()

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
