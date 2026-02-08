cmake_minimum_required(VERSION 3.16)

project(MeetingApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ==================== 1. LiveKit SDK 自动下载与配置 ====================
# 通过 cmake/FetchLiveKit.cmake 自动从 GitHub Releases 下载 SDK
# 修改 LIVEKIT_VERSION 可切换版本号
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchLiveKit.cmake)

# [Debug 配置] - extend 目录下 (如果有 Debug 版 SDK)
set(LIVEKIT_DEBUG_ROOT   "${CMAKE_CURRENT_SOURCE_DIR}/extend/livekit-sdk-windows-x64-debug")
set(LIVEKIT_DEBUG_LIB    "${LIVEKIT_DEBUG_ROOT}/lib")
set(LIVEKIT_DEBUG_BIN    "${LIVEKIT_DEBUG_ROOT}/lib")

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

# ==================== 2. 链接 LiveKit SDK ====================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LIVEKIT_INCLUDE_DIR}
)

# 使用 optimized (Release) 和 debug (Debug) 关键字区分链接库
# Release SDK (通过 cmake/FetchLiveKit.cmake 自动下载) abseil/protobuf 已内嵌到 livekit.dll
target_link_libraries(${PROJECT_NAME} PRIVATE
    # --- Release 模式用 (自动下载的 SDK) ---
    optimized ${LIVEKIT_LIB_DIR}/livekit.lib
    optimized ${LIVEKIT_LIB_DIR}/livekit_ffi.dll.lib
    
    # --- Debug 模式用 ---
    debug ${LIVEKIT_DEBUG_LIB}/livekit.lib
    debug ${LIVEKIT_DEBUG_LIB}/livekit_ffi.dll.lib
    debug ${LIVEKIT_DEBUG_LIB}/libprotobufd.lib
    debug ${LIVEKIT_DEBUG_LIB}/abseil_dll.lib
)

# ==================== 3. 自动复制 DLL 到输出目录 ====================
# 使用生成器表达式 $<IF:$<CONFIG:Debug>, A, B>
# 含义：如果是 Debug 模式，使用路径 A，否则使用路径 B

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    # 1. 复制 livekit.dll (Release SDK 的 bin 目录)
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LIVEKIT_BIN_DIR}/livekit.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    
    # 2. 复制 livekit_ffi.dll (根据构建配置选择 Release/Debug)
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<IF:$<CONFIG:Debug>,${LIVEKIT_DEBUG_BIN}/livekit_ffi.dll,${LIVEKIT_BIN_DIR}/livekit_ffi.dll>"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    
    # 3. 复制 abseil_dll.dll (仅 Debug 需要)
    COMMAND ${CMAKE_COMMAND} -E $<IF:$<CONFIG:Debug>,copy_if_different,true>
        "${LIVEKIT_DEBUG_BIN}/abseil_dll.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        
    # 4. 复制 libprotobufd.dll (仅 Debug 需要)
    COMMAND ${CMAKE_COMMAND} -E $<IF:$<CONFIG:Debug>,copy_if_different,true>
        "${LIVEKIT_DEBUG_BIN}/libprotobufd.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>

    COMMENT "Copying LiveKit SDK DLLs based on build configuration..."
)

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

    # 链接 LiveKit SDK
    target_link_libraries(MeetingAppLib PUBLIC
        optimized ${LIVEKIT_LIB_DIR}/livekit.lib
        optimized ${LIVEKIT_LIB_DIR}/livekit_ffi.dll.lib
        debug ${LIVEKIT_DEBUG_LIB}/livekit.lib
        debug ${LIVEKIT_DEBUG_LIB}/livekit_ffi.dll.lib
        debug ${LIVEKIT_DEBUG_LIB}/libprotobufd.lib
        debug ${LIVEKIT_DEBUG_LIB}/abseil_dll.lib
    )

    if(WIN32)
        target_link_libraries(MeetingAppLib PUBLIC d3d11 dxgi)
    endif()

    # 添加测试子目录
    add_subdirectory(tests)
endif()