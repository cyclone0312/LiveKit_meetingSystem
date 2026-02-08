# =============================================================================
# FetchGoogleTest.cmake
# 自动下载 GoogleTest 并集成到项目中
# =============================================================================

include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
)

# Windows: 防止 GoogleTest 覆盖父项目的编译器/链接器设置
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# 不安装 GoogleTest
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

message(STATUS "GoogleTest v1.15.2 integrated")
