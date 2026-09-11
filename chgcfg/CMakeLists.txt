# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    find_package(SMS++)
endif ()

# -------- chgcfg ----------------------------------------------------------- #
add_executable(chgcfg ../common_utils.cpp chgcfg.cpp)
target_compile_features(chgcfg PUBLIC cxx_std_17)
target_include_directories(chgcfg PRIVATE ..)
target_link_libraries(chgcfg PRIVATE SMS++::SMS++)

install(TARGETS chgcfg
        RUNTIME DESTINATION ${SMSPP_TOOLS_INSTALL_DIR})

set(toolName chgcfg)

configure_file(../cmake/completion.bash.in chgcfg-completion.bash)
configure_file(../cmake/completion.zsh.in chgcfg-completion.zsh)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/chgcfg-completion.bash
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME chgcfg)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/chgcfg-completion.zsh
        DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
        RENAME "_chgcfg")

# --------------------------------------------------------------------------- #
