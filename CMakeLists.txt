# --------------------------------------------------------------------------- #
#    Main CMake file for SMS++ tools                                          #
#                                                                             #
#    This file allows one to build the tests using CMake.                     #
#    To do so, you can use the following commands:                            #
#                                                                             #
#        $ cmake -S <source-path> -B <build-path>                             #
#        $ cmake --build <build-path>                                         #
#                                                                             #
#                                Donato Meoli                                 #
#                         Dipartimento di Informatica                         #
#                             Universita' di Pisa                             #
# --------------------------------------------------------------------------- #
cmake_minimum_required(VERSION 3.21)
cmake_policy(VERSION 3.15)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeriveVersion.cmake)
smspp_derive_version(SMSPP_MODULE_VERSION)

project(SMS++_tools
        VERSION ${SMSPP_MODULE_VERSION}
        DESCRIPTION "SMS++ tools"
        HOMEPAGE_URL https://gitlab.com/smspp/tools
        LANGUAGES C CXX)

# These variables make the code harder to read but easier to change.
set(modName ${PROJECT_NAME})

# C-safe form of modName for the preprocessor macros in <modName>Config.h
set(modMacro "SMSpp_tools")

# Find out if it's being called by the umbrella.
get_directory_property(hasParent PARENT_DIRECTORY)

# Utility function to standardize installation and runtime dependency handling
# for SMS++ tools.
function(smspp_install target_name)
    if (WIN32 AND BUILD_SHARED_LIBS AND NOT DEFINED ENV{CONDA_BUILD})
        # On Windows with shared libraries enabled copies all runtime DLL
        # dependencies next to the executable in the build directory
        # (POST_BUILD), using CMake's TARGET_RUNTIME_DLLS generator expression
        add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target_name}>
                $<TARGET_FILE_DIR:${target_name}>
                COMMAND_EXPAND_LISTS
                COMMENT "Copying runtime DLLs next to ${target_name}.exe")
    endif ()

    install(TARGETS ${target_name}
            RUNTIME DESTINATION ${SMSPP_TOOLS_INSTALL_DIR})
endfunction()

# Installs the config/ directory of a tool, if any, which the tool uses when
# -c is not given and no configuration file is found in the current
# directory, together with the given example files, and generates the man
# page of the tool from its --help if help2man is available.
function(smspp_install_data target_name description)
    set(dataDir ${CMAKE_INSTALL_DATADIR}/${modName}/${target_name})
    set(fullDataDir ${CMAKE_INSTALL_FULL_DATADIR}/${modName}/${target_name})
    set(manFilesText "")

    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config)
        install(DIRECTORY config/ DESTINATION ${dataDir}/config)

        # The tool finds its config/ relative to the directory of the
        # executable, so that the installed tree can be moved as a whole
        file(RELATIVE_PATH configRelDir ${toolsFullDir}
                ${fullDataDir}/config)
        target_compile_definitions(${target_name} PRIVATE
                SMSPP_TOOL_CONFIG_DIR="${configRelDir}")

        string(APPEND manFilesText
                ".TP\n"
                ".I ${fullDataDir}/config/\n"
                "the configuration installed with ${target_name}, used when "
                "\\fB-c\\fR is not given and no configuration file is found "
                "in the current directory\n")
    endif ()

    if (ARGN)
        install(FILES ${ARGN} DESTINATION ${dataDir}/examples)
        string(APPEND manFilesText
                ".TP\n"
                ".I ${fullDataDir}/examples/\n"
                "example input files\n")
    endif ()

    find_program(HELP2MAN help2man)
    if (HELP2MAN)
        set(manPage ${CMAKE_CURRENT_BINARY_DIR}/${target_name}.1)
        set(manFiles ${CMAKE_CURRENT_BINARY_DIR}/${target_name}.h2m)
        set(manInclude "")
        set(manDepends ${target_name})
        if (manFilesText)
            file(WRITE ${manFiles} "[FILES]\n${manFilesText}")
            set(manInclude --include=${manFiles})
            list(APPEND manDepends ${manFiles})
        endif ()
        add_custom_command(OUTPUT ${manPage}
                COMMAND ${HELP2MAN} --no-info --no-discard-stderr
                        --name=${description} ${manInclude}
                        --version-string=${PROJECT_VERSION}
                        --source=${PROJECT_DESCRIPTION}
                        --output=${manPage} $<TARGET_FILE:${target_name}>
                DEPENDS ${manDepends}
                COMMENT "Generating the man page of ${target_name}"
                VERBATIM)
        add_custom_target(${target_name}_man ALL DEPENDS ${manPage})
        install(FILES ${manPage} DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
    endif ()
endfunction()

# ----- Configuration header ------------------------------------------------ #
# This will generate a *Config.h header in the build directory, which every
# tool sees since the build directory is on their include path.
configure_file(cmake/${modName}Config.h.in ${modName}Config.h)
include_directories(${PROJECT_BINARY_DIR})

# ----- Settings ------------------------------------------------------------ #
# The tools are installed in bin/, unless a package manager wants them
# elsewhere (e.g. vcpkg in tools/<port>); every path the tools compute
# relative to their own location starts from there.
include(GNUInstallDirs)
set(SMSPP_TOOLS_INSTALL_DIR ${CMAKE_INSTALL_BINDIR}
    CACHE STRING "Directory where the tools are installed")
cmake_path(ABSOLUTE_PATH SMSPP_TOOLS_INSTALL_DIR
           BASE_DIRECTORY ${CMAKE_INSTALL_PREFIX}
           OUTPUT_VARIABLE toolsFullDir)

# The installed tools look for the SMS++ libraries relative to their own
# location, so that the installed tree can be moved as a whole, and for the
# external libraries where they were found at link time.
if (NOT CMAKE_INSTALL_RPATH)
    if (APPLE)
        set(rpathOrigin "@loader_path")
    else ()
        set(rpathOrigin "$ORIGIN")
    endif ()
    file(RELATIVE_PATH binToLib ${toolsFullDir} ${CMAKE_INSTALL_FULL_LIBDIR})
    set(CMAKE_INSTALL_RPATH "${rpathOrigin}/${binToLib}")
endif ()
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# ----- Tools --------------------------------------------------------------- #
add_subdirectory(block_solver)
add_subdirectory(investmentblock_solver)
add_subdirectory(sddp_solver)
add_subdirectory(tssb_solver)
add_subdirectory(mssb_solver)
add_subdirectory(svm_solver)
add_subdirectory(ucblock_solver)

add_subdirectory(chgcfg)

# --------------------------------------------------------------------------- #
