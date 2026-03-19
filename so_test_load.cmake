add_executable(so_test_load
    test/so_test/so_test_load.c
)

target_link_libraries(so_test_load PRIVATE xsi dl)
