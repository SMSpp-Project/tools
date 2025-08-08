# ----- Settings ------------------------------------------------------------ #
# Since we are using the block factory, objects from linked libraries
# may wrongly appear unused, and by default the linker does not include them,
# so we have to force the linking.
if (BUILD_SHARED_LIBS)
    if (MSVC)
        add_link_options("/WHOLEARCHIVE" "/FORCE:MULTIPLE")
    elseif (UNIX AND (NOT APPLE))
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
# none

# -------- chgcfg ----------------------------------------------------------- #
add_executable(chgcfg chgcfg.cpp)
target_compile_features(chgcfg PUBLIC cxx_std_17)

# ----- Install ------------------------------------------------------------- #
install(TARGETS chgcfg
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ----- Generate and install shell completion scripts ----------------------- #
configure_file(../cmake/completion.bash.in chgcfg-completion.bash)
configure_file(../cmake/completion.zsh.in chgcfg-completion.zsh)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/chgcfg-completion.bash
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME chgcfg)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/chgcfg-completion.zsh
        DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
        RENAME "_chgcfg")

# --------------------------------------------------------------------------- #
