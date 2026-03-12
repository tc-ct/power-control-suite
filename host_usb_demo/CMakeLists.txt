cmake_minimum_required(VERSION 3.12)   # 确保CMake版本支持该策略
project(host_usb_demo)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_policy(SET CMP0074 NEW)          # 启用新行为：使用<PackageName>_ROOT

# 查找hidapi库
find_package(hidapi REQUIRED)

# 添加可执行文件
add_executable(${PROJECT_NAME}
    main.cpp
    usb_driver.cpp
    proto.cpp
    log.cpp
)


target_link_libraries(${PROJECT_NAME} PRIVATE hidapi::hidapi)
target_include_directories(${PROJECT_NAME} PRIVATE ${HIDAPI_INCLUDE_DIRS})