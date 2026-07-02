cmake_minimum_required(VERSION 3.16)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 自动收集所有源文件
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "*.cpp")

# 创建静态库
add_library(op_api_ut_common STATIC ${SOURCES})

set(ASCEND_PATH "${ASCEND_INSTALL_PATH}")
# 设置包含路径（关键配置）
target_include_directories(op_api_ut_common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
set_target_properties(op_api_ut_common PROPERTIES CXX_STANDARD 17)
# 设置编译选项
target_compile_options(op_api_ut_common PRIVATE -Wall -Wextra -fPIC)
target_compile_definitions(op_api_ut_common PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
target_include_directories(op_api_ut_common
    PUBLIC
        ${GTEST_INCLUDE_DIR}
        ${HI_PYTHON_INC}
        ${JSON_ROOT}/include
        ${ASCEND_PATH}/include
        ${ASCEND_PATH}/include/acl
        ${ASCEND_PATH}/include/aclnn
)

# 设置链接库（根据实际需求添加）
target_link_libraries(op_api_ut_common
    PUBLIC
        #${HI_PYTHON_LIB}
        dl
        stdc++
        pthread
        util
    PRIVATE
        # 私有依赖项
)

