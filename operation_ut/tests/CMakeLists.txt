cmake_minimum_required(VERSION 3.16)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(ut-test)

# [IMPORT 环境中CANN包的安装地址]
set(ASCEND_PATH "${ASCEND_INSTALL_PATH}")
# [IMPORT 设置仿真器路径]
set(SIMULATOR_DIR ${ASCEND_PATH}/toolkit/tools/simulator)
# [IMPORT 添加UT测试框架]
add_subdirectory(op_ut)

set(JSON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/op_ut/json")

# 添加src源文件
file(GLOB SRC_FILES
    ${CMAKE_SOURCE_DIR}/src/conv1dsample/*.cce
    ${CMAKE_SOURCE_DIR}/src/postprocsample/*.cce
    ${CMAKE_SOURCE_DIR}/src/convonnxsample/*.cce
)

# 增加测试源文件 按照算子的kernel / api 方式分开
if (${USE_OP_INVOKE_MODE} STREQUAL "kernel")
    file(GLOB TEST_FILES CONFIGURE_DEPENDS
        Ascend310_Test/Test_conv1dkernel/test_*.cpp
        Ascend310_Test/Test_conv1dkernelSt/test_*.cpp
    )
else()
    file(GLOB TEST_FILES CONFIGURE_DEPENDS
        Ascend310_Test/Test_conv1dsample/test_*.cpp
        Ascend310_Test/Test_postprocsample/test_*.cpp
        Ascend310_Test/Test_convonnxsample/test_*.cpp
    )
endif()


#============================================
# 以下配置算子的调用方式为：api
#  op_api() {<<<>>>op()}
#============================================

# CCE 运行在NPU上 生成静态链接库方式
function(func_npu_static_api)
    # 生成 CCE 源码的静态文件  libcce_static.a
    set(CCE_OBJECTS "")
    foreach(SOURCE IN LISTS SRC_FILES)
        get_filename_component(OBJ_NAME "${SOURCE}" NAME_WE)
        set(OBJ_FILE "${CMAKE_CURRENT_BINARY_DIR}/${OBJ_NAME}.o")
        add_custom_command(
            OUTPUT "${OBJ_FILE}"
            COMMAND ccec -c -O2 -fPIC ${_CMAKE_COMPILE_AS_CCE_FLAG}
                  "${SOURCE}"
                  -I"${ASCEND_PATH}/include"
            DEPENDS "${SOURCE}"
            COMMENT "Compiling ${SOURCE} to ${OBJ_FILE}"
        )
        list(APPEND CCE_OBJECTS "${OBJ_FILE}")
    endforeach()
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libcce_static.a"
        COMMAND ccec --cce-fatobj-link ${CCE_OBJECTS}
              -fPIC --cce-build-static-lib
              -o "${CMAKE_CURRENT_BINARY_DIR}/libcce_static.a"
        DEPENDS ${CCE_OBJECTS}
        COMMENT "Building static library libcce_static.a"
    )

    add_custom_target(libcce_static_target ALL
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libcce_static.a"
    )
    # [IMPORT 构建顺序,引入UT测试框架]
    add_dependencies(libcce_static_target op_ut_project)

    # 生成可执行文件
    add_executable(${TEST_CASE_NAME} ${TEST_FILES})

    # [IMPORTANT INFO 构建顺序, 重要配置, 一般不需要改变]
    add_dependencies(${TEST_CASE_NAME} libcce_static_target)

    # 设置编译选项
    target_compile_options(${TEST_CASE_NAME} PRIVATE -O2 -fPIC -c)
    add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)

    target_include_directories(${TEST_CASE_NAME}
        PUBLIC
            ${HI_PYTHON_INC}
        PRIVATE
            ${JSON_ROOT}/include
            ${ASCEND_PATH}/include
            ${ASCEND_PATH}/include/aclnn
    )

    # 配置链接参数
    target_link_directories(${TEST_CASE_NAME}
        PRIVATE
            ${ASCEND_PATH}/lib64
            ${CMAKE_CURRENT_BINARY_DIR}
            #${SIMULATOR_DIR}/Ascend310P3/lib
            ${HI_PYTHON_LIBDIR}
    )
    target_link_libraries(${TEST_CASE_NAME}
        PRIVATE
            cce_static
            op_ut_project # [IMPORT 引入UT测试框架的lib]
            runtime
            stdc++
            pthread
            c_sec
    )
endfunction()

# CCE 运行在NPU上 生成动态链接库方式
function(func_npu_shared_api)
    # 生成 CCE 源码的静态文件  libcce_shared.so
    set(CCE_OBJECTS "")
    foreach(SOURCE IN LISTS SRC_FILES)
        get_filename_component(OBJ_NAME "${SOURCE}" NAME_WE)
        set(OBJ_FILE "${CMAKE_CURRENT_BINARY_DIR}/${OBJ_NAME}.o")
        add_custom_command(
            OUTPUT "${OBJ_FILE}"
            COMMAND ccec -c -O2 -fPIC ${_CMAKE_COMPILE_AS_CCE_FLAG}
                  "${SOURCE}"
                  -I${ASCEND_PATH}/include
                  -o "${OBJ_FILE}"
            DEPENDS "${SOURCE}"
            COMMENT "Compiling ${SOURCE} to ${OBJ_FILE}"
            VERBATIM
        )
        list(APPEND CCE_OBJECTS "${OBJ_FILE}")
    endforeach()

    # 确保有源文件被处理
    if(NOT CCE_OBJECTS)
        message(FATAL_ERROR "No CCE source files found to compile")
    endif()

    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libcce_shared.so"
        COMMAND ccec --cce-fatobj-link ${CCE_OBJECTS}
              -fPIC --shared
              -o "${CMAKE_CURRENT_BINARY_DIR}/libcce_shared.so"
        DEPENDS ${CCE_OBJECTS}
        COMMENT "Building shared library libcce_shared.so"
    )

    add_custom_target(libcce_shared_target ALL
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libcce_shared.so"
    )
    # [IMPORT 构建顺序,引入UT测试框架]
    add_dependencies(libcce_shared_target op_ut_project)

    # 生成可执行文件
    add_executable(${TEST_CASE_NAME} ${TEST_FILES})

    # [IMPORTANT INFO 构建顺序, 重要配置, 一般不需要改变]
    add_dependencies(${TEST_CASE_NAME} libcce_shared_target)

    # 设置编译选项
    target_compile_options(${TEST_CASE_NAME} PRIVATE -O2 -fPIC)
    add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)

    target_include_directories(${TEST_CASE_NAME}
        PUBLIC
            ${HI_PYTHON_INC}
        PRIVATE
            ${JSON_ROOT}/include
            ${ASCEND_PATH}/include
            ${ASCEND_PATH}/include/aclnn
    )

    # 配置链接参数
    target_link_directories(${TEST_CASE_NAME}
        PRIVATE
            ${ASCEND_PATH}/lib64
            ${CMAKE_CURRENT_BINARY_DIR}
            #${SIMULATOR_DIR}/Ascend310P3/lib
            ${HI_PYTHON_LIBDIR}
    )
    target_link_libraries(${TEST_CASE_NAME}
        PRIVATE
            cce_shared
            op_ut_project # [IMPORT 引入UT测试框架的lib]
            runtime
            stdc++
            pthread
            c_sec
            ascendcl
            m
    )
endfunction()

# CCE 运行在 sim ca 仿真器上
function(func_sim_ca_api)
    set(CCE_OBJECTS "")
    foreach(SOURCE IN LISTS SRC_FILES)
        get_filename_component(OBJ_NAME "${SOURCE}" NAME_WE)
        set(OBJ_FILE "${CMAKE_CURRENT_BINARY_DIR}/${OBJ_NAME}.o")
        add_custom_command(
            OUTPUT "${OBJ_FILE}"
            COMMAND ccec -c -O2 -fPIC ${_CMAKE_COMPILE_AS_CCE_FLAG}
                  "${SOURCE}"
                  -I"${ASCEND_PATH}/include"
            DEPENDS "${SOURCE}"
            COMMENT "Compiling ${SOURCE} to ${OBJ_FILE}"
        )
        list(APPEND CCE_OBJECTS "${OBJ_FILE}")
    endforeach()
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libop_kernel.so"
        COMMAND ccec --cce-fatobj-link ${CCE_OBJECTS}
              -fPIC --shared
              -o "${CMAKE_CURRENT_BINARY_DIR}/libop_kernel.so"
        DEPENDS ${CCE_OBJECTS}
        COMMENT "Building shared library libop_kernel.so"
    )

    add_custom_target(op_kernel_target ALL
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libop_kernel.so"
    )

    # [IMPORT 构建顺序,引入UT测试框架]
    add_dependencies(op_kernel_target op_ut_project)

    add_executable(${TEST_CASE_NAME} ${TEST_FILES})

    # [IMPORTANT INFO 构建顺序, 重要配置, 一般不需要改变]
    add_dependencies(${TEST_CASE_NAME} op_kernel_target)

     # 设置编译选项
    target_compile_options(${TEST_CASE_NAME}
        PRIVATE
            ${OPTION_LIST}
            -c -O2 -fPIC
    )
    add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)

    target_include_directories(${TEST_CASE_NAME} PRIVATE
        PUBLIC
            ${HI_PYTHON_INC}
        PRIVATE
            ${JSON_ROOT}/include
            ${ASCEND_PATH}/include
            ${ASCEND_PATH}/include/aclnn
    )

    target_link_directories(${TEST_CASE_NAME} PRIVATE
        ${ASCEND_PATH}/lib64
        $<$<STREQUAL:${ASCEND_RUN_MODE},sim_ca>:${SIMULATOR_DIR}/Ascend310P3/lib>
        ${CMAKE_CURRENT_BINARY_DIR}
        ${HI_PYTHON_LIBDIR}
    )

    target_link_libraries(${TEST_CASE_NAME}
        PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}/libop_kernel.so
            op_ut_project # [IMPORT 引入UT测试框架的lib]
            $<$<STREQUAL:${ASCEND_RUN_MODE},npu>:runtime>
            $<$<STREQUAL:${ASCEND_RUN_MODE},sim_ca>:runtime_camodel> # [ca仿真器加载]
            stdc++
            pthread
            c_sec
            ascendcl
            m

    )
endfunction()

# CCE 仿真器运行在 sim pv上
function(func_sim_pv_api)
    set(CCE_OBJECTS "")
    foreach(SOURCE IN LISTS SRC_FILES)
        get_filename_component(OBJ_NAME "${SOURCE}" NAME_WE)
        set(OBJ_FILE "${CMAKE_CURRENT_BINARY_DIR}/${OBJ_NAME}.o")
        add_custom_command(
            OUTPUT "${OBJ_FILE}"
            COMMAND ccec -c -O2 -fPIC ${_CMAKE_COMPILE_AS_CCE_FLAG}
                  "${SOURCE}"
                  -I"${ASCEND_PATH}/include"
            DEPENDS "${SOURCE}"
            COMMENT "Compiling ${SOURCE} to ${OBJ_FILE}"
        )
        list(APPEND CCE_OBJECTS "${OBJ_FILE}")
    endforeach()
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libop_kernel.a"
        COMMAND ccec --cce-fatobj-link ${CCE_OBJECTS}
              -fPIC --cce-build-static-lib
              -o "${CMAKE_CURRENT_BINARY_DIR}/libop_kernel.a"
        DEPENDS ${CCE_OBJECTS}
        COMMENT "Building static library libop_kernel.a"
    )

    add_custom_target(op_kernel_target ALL
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libop_kernel.a"
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Failed to generate libop_kernel.a")
    endif()

    # [IMPORT 构建顺序,引入UT测试框架]
    add_dependencies(op_kernel_target op_ut_project)

    add_executable(${TEST_CASE_NAME} ${TEST_FILES})

    # [IMPORTANT INFO 构建顺序, 重要配置, 一般不需要改变]
    add_dependencies(${TEST_CASE_NAME} op_kernel_target)
    # 设置编译选项
    target_compile_options(${TEST_CASE_NAME} PRIVATE
        -gdwarf-4
        -D__KLx86__
    )
    add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)

    target_include_directories(${TEST_CASE_NAME} PRIVATE
        PUBLIC
            ${HI_PYTHON_INC}
        PRIVATE
            ${JSON_ROOT}/include
            ${ASCEND_PATH}/include
            ${ASCEND_PATH}/include/aclnn
    )

    target_link_directories(${TEST_CASE_NAME} PRIVATE
        ${ASCEND_PATH}/lib64
        $<$<STREQUAL:${ASCEND_RUN_MODE},sim_pv>:${SIMULATOR_DIR}/Ascend310P3/lib>
        ${CMAKE_CURRENT_BINARY_DIR}
        ${HI_PYTHON_LIBDIR}
    )

    target_link_libraries(${TEST_CASE_NAME} PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/libop_kernel.a
        op_ut_project # [IMPORT 引入UT测试框架的lib]
        $<$<STREQUAL:${ASCEND_RUN_MODE},npu>:runtime>
        $<$<STREQUAL:${ASCEND_RUN_MODE},sim_pv>:runtime_cmodel> # [sim pv仿真器加载]
        ascendcl
        stdc++
        pthread
        c_sec
    )
endfunction()

#============================================
# 以下配置算子的调用方式为：kernel
#  rtDevBinaryRegister->rtFunctionRegister->rtKernelLaunch
#============================================

# CCE 运行在NPU上 生成动态链接库方式
function(func_npu_shared_kernel)
    # 编译CCE代码
    set(CCE_OBJECTS "")
    foreach(SOURCE IN LISTS SRC_FILES)
        get_filename_component(OBJ_NAME "${SOURCE}" NAME_WE)
        set(OBJ_FILE "${CMAKE_CURRENT_BINARY_DIR}/${OBJ_NAME}.o")
        add_custom_command(
            OUTPUT "${OBJ_FILE}"
            COMMAND ccec -c -O2 -fPIC --cce-aicore-only ${_CMAKE_COMPILE_AS_CCE_FLAG}
                  "${SOURCE}"
                  -I"${ASCEND_PATH}/include"
                  -o "${OBJ_FILE}"
            DEPENDS "${SOURCE}"
            COMMENT "Compiling ${SOURCE} to ${OBJ_FILE}"
            VERBATIM
        )
        add_custom_target(${OBJ_NAME}_obj ALL DEPENDS "${OBJ_FILE}")
        list(APPEND CCE_OBJECTS "${OBJ_FILE}")
    endforeach()
    if(NOT CCE_OBJECTS)
        message(FATAL_ERROR "No CCE source files found to compile")
    endif()

    # 编译框架代码
    add_executable(${TEST_CASE_NAME} ${TEST_FILES})
    add_dependencies(${TEST_CASE_NAME} op_ut_project)
    target_compile_options(${TEST_CASE_NAME} PRIVATE -O2 -fPIC)
    add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)
    target_include_directories(${TEST_CASE_NAME}
        PUBLIC
            ${HI_PYTHON_INC}
        PRIVATE
            ${JSON_ROOT}/include
            ${ASCEND_PATH}/include
            ${ASCEND_PATH}/include/aclnn
    )

    # 配置链接参数
    target_link_directories(${TEST_CASE_NAME}
        PRIVATE
            ${ASCEND_PATH}/lib64
            ${CMAKE_CURRENT_BINARY_DIR}
            #${SIMULATOR_DIR}/Ascend310P3/lib
            ${HI_PYTHON_LIBDIR}
    )
    target_link_libraries(${TEST_CASE_NAME}
        PRIVATE
            op_ut_project # [IMPORT 引入UT测试框架的lib]
            runtime
            stdc++
            pthread
            c_sec
            ascendcl
            m
    )

endfunction()

# CCE 运行在 sim ca 仿真器上
function(func_sim_ca_kernel)
    # 编译CCE代码
    set(CCE_OBJECTS "")
    foreach(SOURCE IN LISTS SRC_FILES)
        get_filename_component(OBJ_NAME "${SOURCE}" NAME_WE)
        set(OBJ_FILE "${CMAKE_CURRENT_BINARY_DIR}/${OBJ_NAME}.o")
        add_custom_command(
            OUTPUT "${OBJ_FILE}"
            COMMAND ccec -c -O2 -fPIC --cce-aicore-only ${_CMAKE_COMPILE_AS_CCE_FLAG}
                  "${SOURCE}"
                  -I"${ASCEND_PATH}/include"
                  -o "${OBJ_FILE}"
            DEPENDS "${SOURCE}"
            COMMENT "Compiling ${SOURCE} to ${OBJ_FILE}"
            VERBATIM
        )
        add_custom_target(${OBJ_NAME}_obj ALL DEPENDS "${OBJ_FILE}")
        list(APPEND CCE_OBJECTS "${OBJ_FILE}")
    endforeach()
    if(NOT CCE_OBJECTS)
        message(FATAL_ERROR "No CCE source files found to compile")
    endif()

    # 编译框架代码
    add_executable(${TEST_CASE_NAME} ${TEST_FILES})
    add_dependencies(${TEST_CASE_NAME} op_ut_project)
    target_compile_options(${TEST_CASE_NAME} PRIVATE
        -gdwarf-4
        -DDEBUG
    )
    target_compile_definitions(${TEST_CASE_NAME} PRIVATE
        ${OPTION_LIST}
        _GLIBCXX_USE_CXX11_ABI=0
    )

    #add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)

    target_include_directories(${TEST_CASE_NAME}
        PUBLIC
            ${HI_PYTHON_INC}
        PRIVATE
            ${JSON_ROOT}/include
            ${ASCEND_PATH}/include
            ${ASCEND_PATH}/include/aclnn
    )

    # 配置链接参数
    target_link_directories(${TEST_CASE_NAME}
        PRIVATE
            ${ASCEND_PATH}/lib64
            $<$<STREQUAL:${ASCEND_RUN_MODE},sim_ca>:${SIMULATOR_DIR}/Ascend310P3/lib> # [按实际版型适配]
            ${CMAKE_CURRENT_BINARY_DIR}
            ${HI_PYTHON_LIBDIR}
    )
    target_link_libraries(${TEST_CASE_NAME}
        PRIVATE
            op_ut_project # [IMPORT 引入UT测试框架的lib]
            $<$<STREQUAL:${ASCEND_RUN_MODE},npu>:runtime>
            ascendcl
            stdc++
            $<$<STREQUAL:${ASCEND_RUN_MODE},sim_ca>:runtime_camodel> # [ca仿真器加载]
            op_ut_project # [IMPORT 引入UT测试框架的lib]
            pthread
            c_sec
    )
endfunction()

# CCE 运行在 sim pv 仿真器上
function(func_sim_pv_kernel)
    # 编译CCE代码
    set(CCE_OBJECTS "")
    foreach(SOURCE IN LISTS SRC_FILES)
        get_filename_component(OBJ_NAME "${SOURCE}" NAME_WE)
        set(OBJ_FILE "${CMAKE_CURRENT_BINARY_DIR}/${OBJ_NAME}.o")
        add_custom_command(
            OUTPUT "${OBJ_FILE}"
            COMMAND ccec -c -O2 -fPIC --cce-aicore-only ${_CMAKE_COMPILE_AS_CCE_FLAG}
                  "${SOURCE}"
                  -I"${ASCEND_PATH}/include"
                  -o "${OBJ_FILE}"
            DEPENDS "${SOURCE}"
            COMMENT "Compiling ${SOURCE} to ${OBJ_FILE}"
            VERBATIM
        )
        add_custom_target(${OBJ_NAME}_obj ALL DEPENDS "${OBJ_FILE}")
        list(APPEND CCE_OBJECTS "${OBJ_FILE}")
    endforeach()
    if(NOT CCE_OBJECTS)
        message(FATAL_ERROR "No CCE source files found to compile")
    endif()

    # 编译框架代码
    add_executable(${TEST_CASE_NAME} ${TEST_FILES})
    add_dependencies(${TEST_CASE_NAME} op_ut_project)
    target_compile_options(${TEST_CASE_NAME} PRIVATE
        -gdwarf-4
        -DDEBUG
        -D__KLx86__
    )
    target_compile_definitions(${TEST_CASE_NAME} PRIVATE
        ${OPTION_LIST}
        _GLIBCXX_USE_CXX11_ABI=0
    )

    #add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)

    target_include_directories(${TEST_CASE_NAME}
        PUBLIC
            ${HI_PYTHON_INC}
        PRIVATE
            ${JSON_ROOT}/include
            ${ASCEND_PATH}/include
            ${ASCEND_PATH}/include/aclnn
    )

    # 配置链接参数
    target_link_directories(${TEST_CASE_NAME}
        PRIVATE
            ${ASCEND_PATH}/lib64
            $<$<STREQUAL:${ASCEND_RUN_MODE},sim_pv>:${SIMULATOR_DIR}/Ascend310P3/lib> # [按实际版型适配]
            ${CMAKE_CURRENT_BINARY_DIR}
            ${HI_PYTHON_LIBDIR}
    )
    target_link_libraries(${TEST_CASE_NAME}
        PRIVATE
            op_ut_project # [IMPORT 引入UT测试框架的lib]
            $<$<STREQUAL:${ASCEND_RUN_MODE},npu>:runtime>
            ascendcl
            stdc++
            $<$<STREQUAL:${ASCEND_RUN_MODE},sim_pv>:runtime_cmodel> # [sim_pv仿真]
            op_ut_project # [IMPORT 引入UT测试框架的lib]
            pthread
            c_sec
    )
endfunction()

#============================================
# 控制语句
#============================================
# 根据 ASCEND_RUN_MODE 选择不同的编译链接方式
set(TEST_CASE_NAME ${smoke_testcase}_${ASCEND_RUN_MODE})
# 在CPU上暂不支持
if(${ASCEND_RUN_MODE} STREQUAL "cpu")
    message(FATAL_ERROR "Input RUN_MODE Is Error. Please Re-Enter. Can Be Either sim or npu..")

# 在npu环境上支持：kernel和api(支持静态+动态)
elseif(${ASCEND_RUN_MODE} STREQUAL "npu")
    if (${USE_OP_INVOKE_MODE} STREQUAL "kernel")
        func_npu_shared_kernel()
    elseif(${USE_OP_INVOKE_MODE} STREQUAL "api")
        if(${USE_STATIC_GTEST} STREQUAL "OFF")
            func_npu_shared_api()
        else()
            func_npu_static_api()
        endif()
    else()
        message(FATAL_ERROR "Input USE_OP_INVOKE_MODE Is Error. Please Re-Enter")
    endif()

# 在sim ca仿真上支持：kernel和api
elseif(${ASCEND_RUN_MODE} STREQUAL "sim_ca")
    if (${USE_OP_INVOKE_MODE} STREQUAL "kernel")
        func_sim_ca_kernel()
    elseif(${USE_OP_INVOKE_MODE} STREQUAL "api")
        func_sim_ca_api()
    else()
        message(FATAL_ERROR "Input USE_OP_INVOKE_MODE Is Error. Please Re-Enter")
    endif()

# 在sim pv仿真上支持：kernel和api
elseif(${ASCEND_RUN_MODE} STREQUAL "sim_pv")
    if (${USE_OP_INVOKE_MODE} STREQUAL "kernel")
        func_sim_pv_kernel()
    elseif(${USE_OP_INVOKE_MODE} STREQUAL "api")
        func_sim_pv_api()
    else()
        message(FATAL_ERROR "Input USE_OP_INVOKE_MODE Is Error. Please Re-Enter")
    endif()

# 其他输入默认不支持
else()
    message(FATAL_ERROR "Input RUN_MODE Is Error. Please Re-Enter")
endif()


# 配置测试命令
add_test(NAME ${TEST_CASE_NAME} COMMAND ${TEST_CASE_NAME} --gtest_filter=* )
