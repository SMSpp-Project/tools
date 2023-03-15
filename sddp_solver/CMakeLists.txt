# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    find_package(BundleSolver)
    find_package(LagrangianDualSolver)
    find_package(MILPSolver)
    find_package(SDDPBlock)
    find_package(UCBlock)
endif ()

# ----- sddp_solver --------------------------------------------------------- #
if (TARGET SMS++::UCBlock AND
    TARGET SMS++::SDDPBlock AND
    TARGET SMS++::MILPSolver)

    add_executable(sddp_solver sddp_solver.cpp CutProcessing.cpp)
    target_compile_features(sddp_solver PUBLIC cxx_std_17)
    target_include_directories(sddp_solver PRIVATE ../ucblock_solver)
    target_link_libraries(sddp_solver PRIVATE
                          SMS++::UCBlock
                          SMS++::SDDPBlock
                          SMS++::MILPSolver)

    if (UNIX AND (NOT APPLE))
        target_link_libraries(sddp_solver PRIVATE stdc++fs)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(sddp_solver PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(sddp_solver PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    find_package(Boost COMPONENTS mpi)
    find_package(MPI)
    if (MPI_CXX_FOUND AND Boost_MPI_FOUND)
        message(STATUS "sddp_solver: MPI and Boost::mpi found. Using MPI.")
        target_compile_definitions(sddp_solver PRIVATE USE_MPI)
        target_link_libraries(sddp_solver PRIVATE Boost::mpi MPI::MPI_CXX)
    endif ()

    find_package(OpenMP)
    if (OPENMP_FOUND)
        target_link_libraries(sddp_solver PRIVATE OpenMP::OpenMP_CXX)
    endif ()

    # Install
    install(TARGETS sddp_solver
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # Generate and install shell completion scripts
    configure_file(../cmake/completion.bash.in sddp_solver-completion.bash)
    configure_file(../cmake/completion.zsh.in sddp_solver-completion.zsh)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sddp_solver-completion.bash
            DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
            RENAME sddp_solver)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sddp_solver-completion.zsh
            DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
            RENAME "_sddp_solver")
endif ()

# --------------------------------------------------------------------------- #
