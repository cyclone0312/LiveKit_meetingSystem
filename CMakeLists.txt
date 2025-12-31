cmake_minimum_required(VERSION 3.16)

project(MeetingApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ==================== LiveKit SDK 配置 ====================
set(LIVEKIT_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/livekit-sdk-windows-x64")
set(LIVEKIT_INCLUDE_DIR "${LIVEKIT_SDK_DIR}/include")
set(LIVEKIT_LIB_DIR "${LIVEKIT_SDK_DIR}/lib/windows-x64/release")
set(LIVEKIT_BIN_DIR "${LIVEKIT_SDK_DIR}/bin/release")

# 查找Qt包（添加 Network 模块用于 HTTP 请求，WebSockets 用于 LiveKit 通信，Concurrent 用于后台线程）
find_package(Qt6 REQUIRED COMPONENTS Core Quick QuickControls2 Multimedia Network WebSockets Concurrent)

# 源文件
set(PROJECT_SOURCES
    src/main.cpp
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

# ==================== 链接 LiveKit SDK ====================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LIVEKIT_INCLUDE_DIR}
)

# 链接 LiveKit 库及其所有依赖
# 注意：顺序很重要，被依赖的库要放在后面
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${LIVEKIT_LIB_DIR}/livekit.lib
    ${LIVEKIT_LIB_DIR}/livekit_ffi.dll.lib
    ${LIVEKIT_LIB_DIR}/libprotobuf.lib
    ${LIVEKIT_LIB_DIR}/abseil_dll.lib
)

# ==================== 复制 DLL 到输出目录 ====================
# 编译后自动复制必要的 DLL 文件
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LIVEKIT_BIN_DIR}/livekit_ffi.dll"
        "${LIVEKIT_BIN_DIR}/abseil_dll.dll"
        "${LIVEKIT_BIN_DIR}/libprotobuf.dll"
        "${LIVEKIT_BIN_DIR}/SDL3.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying LiveKit SDK DLLs to output directory..."
)

# Windows特定设置
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# 安装规则
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
