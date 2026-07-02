cmake_minimum_required(VERSION 3.16)
set(CMAKE_SYSTEM_NAME Linux)

# 设置公共变量
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE OFF)

# 设置项目所使用的编译器
#project(Operator_Test_Project LANGUAGES CXX)

# 添加python
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/py.cmake)

# 添加其他变量
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/import_value.cmake)

# 启用测试支持
enable_testing()

# 添加编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-access-control")

# 设置CANN包路径
set(ASCEND_PATH "${ASCEND_INSTALL_PATH}")

# 添加Gtest
#option(USE_STATIC_GTEST "Use static Google Test libraries" ON)
message(STATUS "[INFO] USE_STATIC_GTEST = ${USE_STATIC_GTEST}")
set(GTEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/googletest")
if(${USE_STATIC_GTEST})
    set(GTEST_LIBRARY "${GTEST_ROOT}/gtest/lib/libgtest.a")
    set(GTEST_MAIN_LIBRARY "${GTEST_ROOT}/gtest/lib/libgtest_main.a")
    set(GTEST_INCLUDE_DIR "${GTEST_ROOT}/gtest/include" CACHE PATH "Path to GTest includes")
else()
    set(GTEST_LIBRARY "${GTEST_ROOT}/gtest_shared/lib64/libgtest.so")
    set(GTEST_MAIN_LIBRARY "${GTEST_ROOT}/gtest_shared/lib64/libgtest_main.so")
    set(GTEST_INCLUDE_DIR "${GTEST_ROOT}/gtest_shared/include" CACHE PATH "Path to GTest includes")
endif()
include_directories(${GTEST_INCLUDE_DIR})

# 定义头文件
set(FRINC "${CMAKE_CURRENT_SOURCE_DIR}/framework/include")
include_directories(${FRINC})


# 添加JSON
set(JSON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/json")

# 添加框架
add_subdirectory(framework)

# 创建静态编译库
set(UT_SOURCES ut_main.cpp)
add_library(op_ut_project STATIC ${UT_SOURCES})
# 设置依赖项
add_dependencies(op_ut_project op_api_ut_common)

# 设置编译定义
target_compile_definitions(op_ut_project
    PRIVATE
        OP_API_UT_SRC_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\"
        ENABLE_DEBUG_LOG=${DEBUG_LOG}
        ASCEND_PATH=\"${ASCEND_PATH}\"
        _GLIBCXX_USE_CXX11_ABI=0
)
target_compile_options(op_ut_project PRIVATE -O2 -fPIC -c)
add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)
set_target_properties(op_ut_project PROPERTIES CXX_STANDARD 17)

# 对外发布的文件
set(OP_UT_INCLUDE_PUBLIC_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/framework/include"
    "${GTEST_INCLUDE_DIR}"
    "${JSON_ROOT}/include"
)
target_include_directories(op_ut_project
    PUBLIC
        ${OP_UT_INCLUDE_PUBLIC_DIR}
        ${HI_PYTHON_INC}
    PRIVATE
        ${JSON_ROOT}/include
        ${ASCEND_PATH}/include
        ${ASCEND_PATH}/include/aclnn
)
target_link_libraries(op_ut_project
    PRIVATE
        op_api_ut_common
        ${GTEST_LIBRARY}
        ${GTEST_MAIN_LIBRARY}
        #runtime
        #stdc++
        #pthread
        #c_sec
)
# 设置输出目录（构建时立即生效）
#set_target_properties(op_ut_project PROPERTIES
#    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
#    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
#    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
#)
