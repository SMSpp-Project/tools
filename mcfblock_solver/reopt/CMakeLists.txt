# --------------------------------------------------------------------------- #
#    CMake file of the study on the re-optimization of the Min-Cost Flow      #
#    problem                                                                  #
#                                                                             #
#    A project of its own, not added to the one of tools/: it is configured   #
#    from this directory against an SMS++ build or installation, e.g.,        #
#                                                                             #
#      cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \                       #
#            -DCMAKE_PREFIX_PATH=<the SMS++ build or install directory>       #
#      cmake --build build                                                    #
#                                                                             #
#    and builds the driver reopt_bench, plus reopt_record, which records      #
#    the sequences of a Lagrangian relaxation of the multicommodity flow      #
#    problem, when MMCFBlock, LagrangianDualSolver and BundleSolver are       #
#    there.                                                                   #
#                                                                             #
#                               Donato Meoli                                  #
#                          Dipartimento di Informatica                        #
#                              Universita' di Pisa                            #
#                                                                             #
# --------------------------------------------------------------------------- #
cmake_minimum_required(VERSION 3.19)
project(reopt_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

# the Solver are registered in their factories by static objects, which the
# linker must not drop although nothing in reopt_bench names them
if (UNIX AND (NOT APPLE))
    add_link_options("-Wl,--no-as-needed")
endif ()

# the Find modules of the external libraries: those of SMS++, which an
# installation carries next to its package files and a build tree does not,
# so that there they are taken from the sources of the umbrella project
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../../SMS++/cmake)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../SMS++/cmake)
endif ()

find_package(MCFBlock REQUIRED)
find_package(MCFClassSolver REQUIRED)
find_package(MCFLemonSolver REQUIRED)

add_executable(reopt_bench reopt_bench.cpp)
target_link_libraries(reopt_bench PRIVATE
                      SMS++::MCFBlock
                      SMS++::MCFClassSolver
                      SMS++::MCFLemonSolver)

find_package(MMCFBlock QUIET)
find_package(LagrangianDualSolver QUIET)
find_package(BundleSolver QUIET)
if (MMCFBlock_FOUND AND LagrangianDualSolver_FOUND AND BundleSolver_FOUND)
    add_executable(reopt_record reopt_record.cpp)
    target_link_libraries(reopt_record PRIVATE
                          SMS++::MMCFBlock
                          SMS++::MCFBlock
                          SMS++::MCFClassSolver
                          SMS++::LagrangianDualSolver
                          SMS++::BundleSolver)
else ()
    message(STATUS "reopt_record left out: it needs MMCFBlock, "
                   "LagrangianDualSolver and BundleSolver")
endif ()
