# --------------------------------------------------------------------------- #
#    Main CMake file for SMS++ tools                                          #
#                                                                             #
#                              Niccolo' Iardella                              #
#                          Operations Research Group                          #
#                         Dipartimento di Informatica                         #
#                             Universita' di Pisa                             #
# --------------------------------------------------------------------------- #
cmake_minimum_required(VERSION 3.8...3.15)
project(SMS++_tools VERSION 0.1.0 LANGUAGES CXX)

# ----- Settings ------------------------------------------------------------ #
# Since we are using the block factory, objects from statically linked libraries
# may wrongly appear unused, and by default the linker does not include them,
# so we have to force the linking.

if (NOT BUILD_SHARED_LIBS)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        add_link_options("-Wl,-all_load")
    else()
        add_link_options("-Wl,--whole-archive,--allow-multiple-definition")
    endif ()
endif ()

# ----- Requirements -------------------------------------------------------- #
find_package(UCBlock REQUIRED)
find_package(MILPSolver REQUIRED)

# ----- block_solver -------------------------------------------------------- #
add_executable(block_solver block_solver.cpp)
target_compile_features(block_solver PUBLIC cxx_std_17)

target_link_libraries(block_solver PRIVATE
                      SMS++::UCBlock
                      SMS++::MILPSolver)
