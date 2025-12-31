cmake_minimum_required(VERSION 3.16)

project(MeetingApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找Qt包
find_package(Qt6 REQUIRED COMPONENTS Core Quick QuickControls2 Multimedia)

# 源文件
set(PROJECT_SOURCES
    src/main.cpp
    src/meetingcontroller.cpp
    src/meetingcontroller.h
    src/participantmodel.cpp
    src/participantmodel.h
    src/chatmodel.cpp
    src/chatmodel.h
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
)

# 设置包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
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
