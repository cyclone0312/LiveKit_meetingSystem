cmake_minimum_required(VERSION 3.16)

project(MeetingApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ==================== 1. LiveKit SDK 自动下载与配置 ====================
# 通过 cmake/FetchLiveKit.cmake 自动从 GitHub Releases 下载 SDK
# 支持 Windows / Linux / macOS，x64 / arm64
# 可在 include 之前设置:
#   LIVEKIT_SDK_VERSION  - SDK 版本 (默认 0.3.1)
#   LINKS_SDK_ARCH       - 架构 x64/arm64 (默认 x64)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchLiveKit.cmake)

# [Windows Debug 配置] - extend 目录下 (如果有 Debug 版 SDK，可选)
if(WIN32)
    set(LIVEKIT_DEBUG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/extend/livekit-sdk-windows-x64-debug")
    set(LIVEKIT_DEBUG_LIB  "${LIVEKIT_DEBUG_ROOT}/lib")
    set(LIVEKIT_DEBUG_BIN  "${LIVEKIT_DEBUG_ROOT}/lib")
endif()

# 查找Qt包
find_package(Qt6 REQUIRED COMPONENTS Core Quick QuickControls2 Multimedia Network WebSockets Concurrent)

# 源文件（不含 main.cpp，便于测试复用）
set(LIB_SOURCES
    src/meetingcontroller.cpp
    src/meetingcontroller.h
    src/participantmodel.cpp
    src/participantmodel.h
    src/chatmodel.cpp
    src/chatmodel.h
    src/livekitmanager.cpp
    src/livekitmanager.h
    src/mediacapture.cpp
    src/mediacapture.h
    src/screencapture.cpp
    src/screencapture.h
    src/remotevideorenderer.cpp
    src/remotevideorenderer.h
    src/remoteaudioplayer.cpp
    src/remoteaudioplayer.h
    src/aiassistant.cpp
    src/aiassistant.h
)

# 源文件（包含 main.cpp）
set(PROJECT_SOURCES
    src/main.cpp
    ${LIB_SOURCES}
)

# QML资源文件
qt_add_resources(QML_RESOURCES resources/qml.qrc)

# 创建可执行文件
qt_add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
    ${QML_RESOURCES}
)

# 链接Qt库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Network
    Qt6::WebSockets
    Qt6::Concurrent
)

# ==================== 2. 链接 LiveKit SDK (跨平台) ====================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LIVEKIT_INCLUDE_DIR}
)

# 使用 Imported Targets 链接（跨平台，由 FetchLiveKit.cmake 创建）
target_link_libraries(${PROJECT_NAME} PRIVATE
    LiveKit::livekit
    LiveKit::livekit_ffi
)

# [Windows] 可选的 Debug SDK 配置（使用 extend/ 下的 Debug 版 SDK）
if(WIN32 AND DEFINED LIVEKIT_DEBUG_LIB AND EXISTS "${LIVEKIT_DEBUG_LIB}/livekit.lib")
    # 为 Imported Targets 添加 DEBUG 配置
    set_property(TARGET LiveKit::livekit APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
    set_target_properties(LiveKit::livekit PROPERTIES
        IMPORTED_IMPLIB_DEBUG   "${LIVEKIT_DEBUG_LIB}/livekit.lib"
        IMPORTED_LOCATION_DEBUG "${LIVEKIT_DEBUG_BIN}/livekit.dll"
    )
    set_property(TARGET LiveKit::livekit_ffi APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
    set_target_properties(LiveKit::livekit_ffi PROPERTIES
        IMPORTED_IMPLIB_DEBUG   "${LIVEKIT_DEBUG_LIB}/livekit_ffi.dll.lib"
        IMPORTED_LOCATION_DEBUG "${LIVEKIT_DEBUG_BIN}/livekit_ffi.dll"
    )
    # Debug 版未内嵌 abseil/protobuf，需要额外链接
    target_link_libraries(${PROJECT_NAME} PRIVATE
        debug "${LIVEKIT_DEBUG_LIB}/libprotobufd.lib"
        debug "${LIVEKIT_DEBUG_LIB}/abseil_dll.lib"
    )
endif()

# ==================== 3. 自动复制运行时库到输出目录 (跨平台) ====================
# 使用 FetchLiveKit.cmake 提供的辅助函数复制 Release 运行时库
livekit_copy_runtime_libs(${PROJECT_NAME})

# [Windows Debug] 额外复制 Debug 版本的 DLL
if(WIN32 AND DEFINED LIVEKIT_DEBUG_BIN AND EXISTS "${LIVEKIT_DEBUG_BIN}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        # Debug 模式下覆盖 livekit_ffi.dll 为 Debug 版本
        COMMAND ${CMAKE_COMMAND} -E $<IF:$<CONFIG:Debug>,copy_if_different,true>
            "${LIVEKIT_DEBUG_BIN}/livekit_ffi.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        # Debug 模式额外依赖: abseil_dll.dll
        COMMAND ${CMAKE_COMMAND} -E $<IF:$<CONFIG:Debug>,copy_if_different,true>
            "${LIVEKIT_DEBUG_BIN}/abseil_dll.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        # Debug 模式额外依赖: libprotobufd.dll
        COMMAND ${CMAKE_COMMAND} -E $<IF:$<CONFIG:Debug>,copy_if_different,true>
            "${LIVEKIT_DEBUG_BIN}/libprotobufd.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "Copying Debug-only DLLs..."
    )
endif()

# Windows特定设置
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE false
    )
    
    # Windows 屏幕捕获依赖（DXGI Desktop Duplication）
    target_link_libraries(${PROJECT_NAME} PRIVATE
        d3d11
        dxgi
    )
endif()

# 安装规则
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ==================== 4. 测试配置（GoogleTest）====================
option(BUILD_TESTS "Build unit tests and integration tests" OFF)

if(BUILD_TESTS)
    message(STATUS "======================================================")
    message(STATUS "  Tests enabled (GoogleTest)")
    message(STATUS "======================================================")

    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchGoogleTest.cmake)

    # 创建静态库目标（供测试链接，避免重复编译）
    qt_add_library(MeetingAppLib STATIC ${LIB_SOURCES})

    target_include_directories(MeetingAppLib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${LIVEKIT_INCLUDE_DIR}
    )

    target_link_libraries(MeetingAppLib PUBLIC
        Qt6::Core
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Multimedia
        Qt6::Network
        Qt6::WebSockets
        Qt6::Concurrent
    )

    # 链接 LiveKit SDK (跨平台 Imported Targets)
    target_link_libraries(MeetingAppLib PUBLIC
        LiveKit::livekit
        LiveKit::livekit_ffi
    )

    # [Windows] Debug SDK 额外依赖
    if(WIN32 AND DEFINED LIVEKIT_DEBUG_LIB AND EXISTS "${LIVEKIT_DEBUG_LIB}/libprotobufd.lib")
        target_link_libraries(MeetingAppLib PUBLIC
            debug "${LIVEKIT_DEBUG_LIB}/libprotobufd.lib"
            debug "${LIVEKIT_DEBUG_LIB}/abseil_dll.lib"
        )
    endif()

    if(WIN32)
        target_link_libraries(MeetingAppLib PUBLIC d3d11 dxgi)
    endif()

    # 添加测试子目录
    add_subdirectory(tests)
endif()