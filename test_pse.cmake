add_executable(test_pse
    test/test_pse/test_pse_main.c
    test/test_pse/pse_tests.c
    test/fbaccess.c
    test/test_pse/test_helpers.c
    test/test_pse/test_master.c
    test/test_pse/test_config.c
)

target_link_libraries(test_pse PRIVATE xsi dl)

set(TARGET_TEST_SCRIPT "${CMAKE_SOURCE_DIR}/test/test_pse/test_pse_times.sh")
set(TEST_SCRIPT_LINK_NAME "${CMAKE_BINARY_DIR}/test_pse_times.sh")

# Add a custom target to create the symlink
ADD_CUSTOM_TARGET(create_test_script_symlink ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${TARGET_TEST_SCRIPT}
    ${TEST_SCRIPT_LINK_NAME}
    COMMENT "Creating symbolic link: ${TEST_SCRIPT_LINK_NAME} -> ${TARGET_TEST_SCRIPT}"
)

set(TARGET_TEST_CONF "${CMAKE_SOURCE_DIR}/test/test_pse/test_pse.conf")
set(TEST_CONF_LINK_NAME "${CMAKE_BINARY_DIR}/test_pse.conf")

ADD_CUSTOM_TARGET(create_test_conf_symlink ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${TARGET_TEST_CONF}
    ${TEST_CONF_LINK_NAME}
    COMMENT "Creating symbolic link: ${TEST_CONF_LINK_NAME} -> ${TARGET_TEST_CONF}"
)

