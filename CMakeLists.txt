cmake_minimum_required(VERSION 3.15.0)
project(ps_engine VERSION 0.1.0 LANGUAGES C CXX)

SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fPIC -g -DAMD64 -std=gnu++11 -fno-rtti -Wl,-z,defs")

set(XSI_PATH ${CMAKE_SOURCE_DIR}/../xsi)
add_subdirectory(${XSI_PATH} ${CMAKE_BINARY_DIR}/xsi)

include_directories(${XSI_PATH}/include/)
include_directories(${XSI_PATH}/include/xsi/)
include_directories(include)

include(so_test_load.cmake)
include(ps_engine.cmake)
include(ps_module.cmake)
include(test_pse.cmake)
 