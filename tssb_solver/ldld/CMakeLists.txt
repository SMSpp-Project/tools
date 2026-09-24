# --------------------------------------------------------------------------- #
#    CMake file of the study on the nested and the recursive Lagrangian dual  #
#                                                                             #
#    A project of its own, not added to the one of tools/: it is configured  #
#    from this directory against an SMS++ build or installation, e.g.,       #
#                                                                             #
#      cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \                       #
#            -DCMAKE_PREFIX_PATH=<the SMS++ build or install directory>       #
#      cmake --build build                                                    #
#                                                                             #
#    and builds the driver ldld_bench alone.                                  #
#                                                                             #
#                              Antonio Frangioni                              #
#                               Donato Meoli                                  #
#                          Dipartimento di Informatica                        #
#                              Universita' di Pisa                            #
#                                                                             #
# --------------------------------------------------------------------------- #
cmake_minimum_required(VERSION 3.19)
project(ldld_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

# the Solver are registered in their factories by static objects, which the
# linker must not drop although nothing in ldld_bench names them
if (UNIX AND (NOT APPLE))
    add_link_options("-Wl,--no-as-needed")
endif ()

# the Find modules of the external libraries: those of SMS++, which an
# installation carries next to its package files and a build tree does not,
# so that there they are taken from the sources of the umbrella project
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../../SMS++/cmake)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../SMS++/cmake)
endif ()

find_package(TwoStageStochasticBlock REQUIRED)
find_package(MultiStageStochasticBlock REQUIRED)
find_package(UCBlock REQUIRED)
find_package(LagrangianDualSolver REQUIRED)
find_package(BundleSolver REQUIRED)
find_package(MILPSolver REQUIRED)
find_package(CapacitatedFacilityLocationBlock REQUIRED)
find_package(BinaryKnapsackBlock REQUIRED)

add_executable(ldld_bench ldld_bench.cpp)
target_link_libraries(ldld_bench PRIVATE
                      SMS++::TwoStageStochasticBlock
                      SMS++::MultiStageStochasticBlock
                      SMS++::UCBlock
                      SMS++::LagrangianDualSolver
                      SMS++::BundleSolver
                      SMS++::MILPSolver
                      SMS++::CapacitatedFacilityLocationBlock
                      SMS++::BinaryKnapsackBlock)

# the generator of the stochastic facility location instances
add_executable(cfl_tssb_gen gen/cfl_tssb_gen.cpp)
target_link_libraries(cfl_tssb_gen PRIVATE
                      SMS++::CapacitatedFacilityLocationBlock
                      SMS++::TwoStageStochasticBlock)
