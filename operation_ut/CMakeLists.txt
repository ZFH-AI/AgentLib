# 主目录 cmake配置文件
cmake_minimum_required(VERSION 3.16)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE OFF)

# [IMPORT 添加 cmake 模块路径]
set(CCE_CMAKE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)
list(APPEND CMAKE_MODULE_PATH ${CCE_CMAKE_PATH})
message(STATUS "[INFO] CCE_CMAKE_PATH = ${CCE_CMAKE_PATH}")
project(test LANGUAGES CCE CXX)

# [IMPORT 添加加载不同的平台的cmake文件]
message(STATUS "INFO: ASCEND_RUN_MOD=${ASCEND_RUN_MODE}")
if(${ASCEND_RUN_MODE} STREQUAL "cpu")
    add_subdirectory(cmake/cpu)
elseif(${ASCEND_RUN_MODE} STREQUAL "npu")
    add_subdirectory(cmake/npu)
elseif(${ASCEND_RUN_MODE} STREQUAL "sim_ca" OR ${ASCEND_RUN_MODE} STREQUAL "sim_pv")
    add_subdirectory(cmake/sim)
else()
    message(FATAL_ERROR "Input RUN_MODE Is Error. Please Re-Enter")
endif()

# [IMPORT 需要在主目录添加Python的cmake配置脚本]
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/py.cmake)

# 添加头文件
add_subdirectory(include)
# 添加源码
add_subdirectory(src)
# 添加测试
add_subdirectory(tests)
