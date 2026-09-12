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

smspp_install(chgcfg)
smspp_install_data(chgcfg "change parameters of an SMS++ configuration file")

# --------------------------------------------------------------------------- #
