# ----- Settings ------------------------------------------------------------ #
# The classes of the modules register themselves in the factories from static
# objects, which nothing in the tool names: a shared library is kept by what
# the module itself asks of the linker [see its CMakeLists.txt], while an
# archive has to be taken whole, or those objects are dropped.
if (NOT BUILD_SHARED_LIBS)
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
    # the InvestmentBlock, whose types of assets it writes, and the UCBlock,
    # whose units give the design Variable the first-stage paths are read
    # against
    find_package(InvestmentBlock)
    find_package(UCBlock)
endif ()

# ----- design_converter ---------------------------------------------------- #
if (TARGET SMS++::InvestmentBlock AND TARGET SMS++::UCBlock)

    add_executable(design_converter design_converter.cpp)
    target_compile_features(design_converter PUBLIC cxx_std_20)
    target_link_libraries(design_converter PRIVATE SMS++::InvestmentBlock
                          SMS++::UCBlock)

    smspp_install(design_converter)
    smspp_install_data(design_converter
            "convert a Block in design mode into an InvestmentBlock")

endif ()

# --------------------------------------------------------------------------- #
