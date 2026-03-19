add_library(ps_engine SHARED
    src/ps_engine/ps_engine_main.cpp 
    src/ps_engine/ps_engine.cpp  
    src/ps_engine/pse_function.cpp
)

target_compile_options(ps_engine PRIVATE -Wno-deprecated)
target_link_libraries(ps_engine PRIVATE xsi)
target_link_libraries(ps_engine PRIVATE dl)

target_link_options(ps_engine PRIVATE 
    "-Wl,--version-script=${CMAKE_SOURCE_DIR}/src/ps_engine/linker_script.exp"
)