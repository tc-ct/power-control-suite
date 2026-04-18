cmake_minimum_required(VERSION 3.12)   # 确保CMake版本支持该策略
project(host_usb_demo)

# 追加路径（保留原有内容）
list(APPEND CMAKE_PREFIX_PATH "./third_party/")


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_policy(SET CMP0074 NEW)          # 启用新行为：使用<PackageName>_ROOT

set(HAL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/hal")

# 查找hidapi库
find_package(hidapi REQUIRED)



# 添加可执行文件
add_executable(${PROJECT_NAME}
    main.cpp
    log.cpp
    ${HAL_SRC_DIR}/file_parse.cpp
    ${HAL_SRC_DIR}/power_control.cpp
    ${HAL_SRC_DIR}/stm32_comm.cpp
    ${HAL_SRC_DIR}/usb_driver.cpp
)


target_link_libraries(${PROJECT_NAME} PRIVATE hidapi::hidapi)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${HIDAPI_INCLUDE_DIRS}
    ./third_party
    ${HAL_SRC_DIR}
)


add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/third_party/bin/hidapi.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)
