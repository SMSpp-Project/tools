# ----- Settings ------------------------------------------------------------ #
# none

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
