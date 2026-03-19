add_library(ps_module SHARED
    src/ps_module/ps_module_main.cpp
    src/ps_module/my_sum_args.cpp
    src/ps_module/fb_iface.cpp     
)

target_link_options(ps_module PRIVATE 
    "-Wl,--version-script=${CMAKE_SOURCE_DIR}/src/ps_module/module_linker_script.exp"
)
