cmake_minimum_required(VERSION 3.16)

project(power_control VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 追加路径（保留原有内容）
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../common/third_party/")
set(HAL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/hal")
set(THIRD_PARTY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../common/third_party/")

# 查找hidapi库
find_package(hidapi REQUIRED)


find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mainwindow.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mainwindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/device_session_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sampling_state_machine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug_window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/waveform_widget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/waveform_recorder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/log.cpp
    ${HAL_SRC_DIR}/file_parse.cpp
    ${HAL_SRC_DIR}/power_control.cpp
    ${HAL_SRC_DIR}/stm32_comm.cpp
    ${HAL_SRC_DIR}/usb_driver.cpp
)

set(PROJECT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${HAL_SRC_DIR}
    ${THIRD_PARTY_PATH}
    ${HIDAPI_INCLUDE_DIRS}
)


if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(power_control
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET power_control APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(power_control SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(power_control
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(power_control PRIVATE Qt${QT_VERSION_MAJOR}::Widgets hidapi::hidapi)
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_INCLUDE_DIRS})


# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.power_control)
endif()
set_target_properties(power_control PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS power_control
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(power_control)
endif()



add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${THIRD_PARTY_PATH}/bin/hidapi.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config/power_config.json"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config/power_config.json")



