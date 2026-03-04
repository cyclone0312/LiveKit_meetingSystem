# =============================================================================
# FetchLiveKit.cmake
# 自动从 GitHub Releases 下载 LiveKit C++ SDK 并配置
# 支持 Windows / Linux / macOS，x64 / arm64
# =============================================================================

# 版本配置（可在 include 之前覆盖）
if(NOT DEFINED LIVEKIT_SDK_VERSION)
    set(LIVEKIT_SDK_VERSION "0.3.1")
endif()

# =============================================================================
# 平台检测（必须在架构检测之前，macOS 需要据此设置默认架构）
# =============================================================================
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LIVEKIT_PLATFORM "windows")
    set(LIVEKIT_ARCHIVE_EXT "zip")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LIVEKIT_PLATFORM "linux")
    set(LIVEKIT_ARCHIVE_EXT "tar.gz")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(LIVEKIT_PLATFORM "macos")
    set(LIVEKIT_ARCHIVE_EXT "tar.gz")
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# 架构配置（可选 x64, arm64）
# macOS 上 GitHub Actions macos-latest 为 Apple Silicon (arm64)，
# 且 LiveKit 官方只发布了 macos-arm64 包，因此 macOS 默认 arm64。
if(NOT DEFINED LINKS_SDK_ARCH)
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(LINKS_SDK_ARCH "arm64")
    else()
        set(LINKS_SDK_ARCH "x64")
    endif()
endif()
string(TOLOWER "${LINKS_SDK_ARCH}" LIVEKIT_ARCH)
if(NOT LIVEKIT_ARCH MATCHES "^(x64|arm64)$")
    message(FATAL_ERROR "Unsupported LINKS_SDK_ARCH: ${LINKS_SDK_ARCH}. Expected x64 or arm64.")
endif()

# =============================================================================
# SDK 路径配置
# =============================================================================
set(LIVEKIT_SDK_NAME "livekit-sdk-${LIVEKIT_PLATFORM}-${LIVEKIT_ARCH}-${LIVEKIT_SDK_VERSION}")
set(LIVEKIT_SDK_ROOT "${CMAKE_SOURCE_DIR}/third_party/${LIVEKIT_SDK_NAME}")
set(LIVEKIT_SDK_ARCHIVE "${LIVEKIT_SDK_NAME}.${LIVEKIT_ARCHIVE_EXT}")
set(LIVEKIT_SDK_URL
    "https://github.com/livekit/client-sdk-cpp/releases/download/v${LIVEKIT_SDK_VERSION}/${LIVEKIT_SDK_ARCHIVE}")

# 下载缓存
set(LIVEKIT_DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/third_party/.cache")
set(LIVEKIT_DOWNLOAD_PATH "${LIVEKIT_DOWNLOAD_DIR}/${LIVEKIT_SDK_ARCHIVE}")
set(LIVEKIT_STAMP_FILE "${LIVEKIT_SDK_ROOT}/.livekit-${LIVEKIT_SDK_VERSION}.stamp")

# =============================================================================
# 下载与解压
# =============================================================================
if(NOT EXISTS "${LIVEKIT_STAMP_FILE}")
    message(STATUS "======================================================")
    message(STATUS "  LiveKit SDK v${LIVEKIT_SDK_VERSION} (${LIVEKIT_PLATFORM}-${LIVEKIT_ARCH})")
    message(STATUS "  URL: ${LIVEKIT_SDK_URL}")
    message(STATUS "======================================================")

    # 创建缓存目录
    file(MAKE_DIRECTORY "${LIVEKIT_DOWNLOAD_DIR}")

    # 下载（如果缓存不存在）
    if(NOT EXISTS "${LIVEKIT_DOWNLOAD_PATH}")
        message(STATUS "Downloading LiveKit SDK v${LIVEKIT_SDK_VERSION} ...")
        file(DOWNLOAD
            "${LIVEKIT_SDK_URL}"
            "${LIVEKIT_DOWNLOAD_PATH}"
            SHOW_PROGRESS
            STATUS DOWNLOAD_STATUS
            TLS_VERIFY ON
        )
        list(GET DOWNLOAD_STATUS 0 DOWNLOAD_RESULT)
        list(GET DOWNLOAD_STATUS 1 DOWNLOAD_ERROR)
        if(NOT DOWNLOAD_RESULT EQUAL 0)
            file(REMOVE "${LIVEKIT_DOWNLOAD_PATH}")
            message(FATAL_ERROR
                "Failed to download LiveKit SDK: ${DOWNLOAD_ERROR}\n"
                "URL: ${LIVEKIT_SDK_URL}\n"
                "Please check your network or download manually and extract to:\n"
                "  ${LIVEKIT_SDK_ROOT}")
        endif()
        message(STATUS "Download complete!")
    else()
        message(STATUS "Using cached archive: ${LIVEKIT_DOWNLOAD_PATH}")
    endif()

    # 清理旧的 SDK 目录
    if(EXISTS "${LIVEKIT_SDK_ROOT}")
        message(STATUS "Removing old SDK directory: ${LIVEKIT_SDK_ROOT}")
        file(REMOVE_RECURSE "${LIVEKIT_SDK_ROOT}")
    endif()

    # 解压到临时目录
    set(LIVEKIT_EXTRACT_TEMP "${LIVEKIT_DOWNLOAD_DIR}/_extract_temp")
    if(EXISTS "${LIVEKIT_EXTRACT_TEMP}")
        file(REMOVE_RECURSE "${LIVEKIT_EXTRACT_TEMP}")
    endif()
    file(MAKE_DIRECTORY "${LIVEKIT_EXTRACT_TEMP}")

    message(STATUS "Extracting LiveKit SDK ...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf "${LIVEKIT_DOWNLOAD_PATH}"
        WORKING_DIRECTORY "${LIVEKIT_EXTRACT_TEMP}"
        RESULT_VARIABLE EXTRACT_RESULT
    )
    if(NOT EXTRACT_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to extract LiveKit SDK archive!")
    endif()

    # 处理解压后可能的嵌套目录
    file(GLOB EXTRACTED_CONTENTS "${LIVEKIT_EXTRACT_TEMP}/*")
    list(LENGTH EXTRACTED_CONTENTS CONTENT_COUNT)
    if(CONTENT_COUNT EQUAL 1 AND IS_DIRECTORY "${EXTRACTED_CONTENTS}")
        set(EXTRACTED_SDK_DIR "${EXTRACTED_CONTENTS}")
    else()
        set(EXTRACTED_SDK_DIR "${LIVEKIT_EXTRACT_TEMP}")
    endif()

    # 移动到最终目标目录
    file(RENAME "${EXTRACTED_SDK_DIR}" "${LIVEKIT_SDK_ROOT}")

    # 清理临时解压目录
    if(EXISTS "${LIVEKIT_EXTRACT_TEMP}")
        file(REMOVE_RECURSE "${LIVEKIT_EXTRACT_TEMP}")
    endif()

    # 写入 stamp 文件标记成功
    file(WRITE "${LIVEKIT_STAMP_FILE}"
        "LiveKit SDK v${LIVEKIT_SDK_VERSION} (${LIVEKIT_PLATFORM}-${LIVEKIT_ARCH})\n")

    message(STATUS "LiveKit SDK v${LIVEKIT_SDK_VERSION} installed at: ${LIVEKIT_SDK_ROOT}")
else()
    message(STATUS "LiveKit SDK v${LIVEKIT_SDK_VERSION} already exists, skipping download.")
endif()

# =============================================================================
# 平台相关的库路径
# =============================================================================
set(LIVEKIT_INCLUDE_DIR "${LIVEKIT_SDK_ROOT}/include" CACHE PATH "LiveKit SDK include directory" FORCE)
set(LIVEKIT_LIB_DIR     "${LIVEKIT_SDK_ROOT}/lib"     CACHE PATH "LiveKit SDK library directory" FORCE)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LIVEKIT_LIBRARY         "${LIVEKIT_SDK_ROOT}/lib/livekit.lib")
    set(LIVEKIT_FFI_LIBRARY     "${LIVEKIT_SDK_ROOT}/lib/livekit_ffi.dll.lib")
    set(LIVEKIT_BIN_DIR         "${LIVEKIT_SDK_ROOT}/bin" CACHE PATH "LiveKit SDK runtime directory" FORCE)
    set(LIVEKIT_RUNTIME_LIB     "${LIVEKIT_BIN_DIR}/livekit.dll")
    set(LIVEKIT_FFI_RUNTIME_LIB "${LIVEKIT_BIN_DIR}/livekit_ffi.dll")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LIVEKIT_LIBRARY         "${LIVEKIT_SDK_ROOT}/lib/liblivekit.so")
    set(LIVEKIT_FFI_LIBRARY     "${LIVEKIT_SDK_ROOT}/lib/liblivekit_ffi.so")
    set(LIVEKIT_BIN_DIR         "${LIVEKIT_SDK_ROOT}/lib" CACHE PATH "LiveKit SDK runtime directory" FORCE)
    set(LIVEKIT_RUNTIME_LIB     "${LIVEKIT_LIBRARY}")
    set(LIVEKIT_FFI_RUNTIME_LIB "${LIVEKIT_FFI_LIBRARY}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(LIVEKIT_LIBRARY         "${LIVEKIT_SDK_ROOT}/lib/liblivekit.dylib")
    set(LIVEKIT_FFI_LIBRARY     "${LIVEKIT_SDK_ROOT}/lib/liblivekit_ffi.dylib")
    set(LIVEKIT_BIN_DIR         "${LIVEKIT_SDK_ROOT}/lib" CACHE PATH "LiveKit SDK runtime directory" FORCE)
    set(LIVEKIT_RUNTIME_LIB     "${LIVEKIT_LIBRARY}")
    set(LIVEKIT_FFI_RUNTIME_LIB "${LIVEKIT_FFI_LIBRARY}")
endif()

# =============================================================================
# macOS: 修复 install_name 路径
# =============================================================================
if(APPLE)
    if(EXISTS "${LIVEKIT_LIBRARY}")
        execute_process(
            COMMAND install_name_tool -id "@rpath/liblivekit.dylib" "${LIVEKIT_LIBRARY}"
            RESULT_VARIABLE _result ERROR_VARIABLE _error
        )
        if(NOT _result EQUAL 0)
            message(WARNING "Failed to normalize install_name for livekit: ${_error}")
        endif()
    endif()

    if(EXISTS "${LIVEKIT_FFI_LIBRARY}")
        execute_process(
            COMMAND install_name_tool -id "@rpath/liblivekit_ffi.dylib" "${LIVEKIT_FFI_LIBRARY}"
            RESULT_VARIABLE _result ERROR_VARIABLE _error
        )
        if(NOT _result EQUAL 0)
            message(WARNING "Failed to normalize install_name for livekit_ffi: ${_error}")
        endif()

        # 修复 livekit_ffi 对 livekit 的绝对路径依赖
        execute_process(
            COMMAND otool -L "${LIVEKIT_FFI_LIBRARY}"
            OUTPUT_VARIABLE _deps_output ERROR_QUIET
        )
        string(REGEX MATCH "(/[^\n ]*liblivekit\\.dylib)" _abs_dep "${_deps_output}")
        if(NOT _abs_dep STREQUAL "")
            execute_process(
                COMMAND install_name_tool -change "${_abs_dep}" "@rpath/liblivekit.dylib" "${LIVEKIT_FFI_LIBRARY}"
                RESULT_VARIABLE _result ERROR_VARIABLE _error
            )
            if(NOT _result EQUAL 0)
                message(WARNING "Failed to rewrite livekit_ffi dependency path: ${_error}")
            endif()
        endif()
    endif()
endif()

# =============================================================================
# 验证关键文件
# =============================================================================
foreach(_check_file
    "${LIVEKIT_INCLUDE_DIR}/livekit/livekit.h"
    "${LIVEKIT_LIBRARY}"
    "${LIVEKIT_FFI_LIBRARY}"
)
    if(NOT EXISTS "${_check_file}")
        message(WARNING "LiveKit SDK file missing: ${_check_file}")
    endif()
endforeach()

# =============================================================================
# 创建 Imported Targets
# =============================================================================
if(NOT TARGET LiveKit::livekit)
    add_library(LiveKit::livekit SHARED IMPORTED)
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set_target_properties(LiveKit::livekit PROPERTIES
            IMPORTED_IMPLIB   "${LIVEKIT_LIBRARY}"
            IMPORTED_LOCATION "${LIVEKIT_RUNTIME_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIVEKIT_INCLUDE_DIR}"
        )
    else()
        set_target_properties(LiveKit::livekit PROPERTIES
            IMPORTED_LOCATION "${LIVEKIT_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIVEKIT_INCLUDE_DIR}"
        )
    endif()
endif()

if(NOT TARGET LiveKit::livekit_ffi)
    add_library(LiveKit::livekit_ffi SHARED IMPORTED)
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set_target_properties(LiveKit::livekit_ffi PROPERTIES
            IMPORTED_IMPLIB   "${LIVEKIT_FFI_LIBRARY}"
            IMPORTED_LOCATION "${LIVEKIT_FFI_RUNTIME_LIB}"
        )
    else()
        set_target_properties(LiveKit::livekit_ffi PROPERTIES
            IMPORTED_LOCATION "${LIVEKIT_FFI_LIBRARY}"
        )
    endif()
endif()

# =============================================================================
# find_package 支持
# =============================================================================
list(APPEND CMAKE_PREFIX_PATH "${LIVEKIT_SDK_ROOT}/lib/cmake/LiveKit")

# =============================================================================
# 辅助函数：复制运行时库到构建输出目录
# =============================================================================
function(livekit_copy_runtime_libs target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LIVEKIT_RUNTIME_LIB}"
            $<TARGET_FILE_DIR:${target_name}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LIVEKIT_FFI_RUNTIME_LIB}"
            $<TARGET_FILE_DIR:${target_name}>
        COMMENT "Copying LiveKit runtime libraries to output directory..."
    )
endfunction()

# =============================================================================
# 配置摘要
# =============================================================================
message(STATUS "------------------------------------------------------")
message(STATUS "  LiveKit SDK v${LIVEKIT_SDK_VERSION} (${LIVEKIT_PLATFORM}-${LIVEKIT_ARCH})")
message(STATUS "  Root:    ${LIVEKIT_SDK_ROOT}")
message(STATUS "  Include: ${LIVEKIT_INCLUDE_DIR}")
message(STATUS "  Library: ${LIVEKIT_LIBRARY}")
message(STATUS "  FFI:     ${LIVEKIT_FFI_LIBRARY}")
message(STATUS "  Bin:     ${LIVEKIT_BIN_DIR}")
message(STATUS "------------------------------------------------------")
