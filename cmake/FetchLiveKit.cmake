# =============================================================================
# FetchLiveKit.cmake
# 自动从 GitHub Releases 下载 LiveKit C++ SDK 并解压到 extend 目录
# =============================================================================

# LiveKit SDK 版本配置
set(LIVEKIT_VERSION "0.2.7" CACHE STRING "LiveKit SDK version to download")
set(LIVEKIT_SDK_DIR "${CMAKE_SOURCE_DIR}/extend/livekit-sdk-windows" CACHE PATH "LiveKit SDK install directory")

# 下载 URL
set(LIVEKIT_DOWNLOAD_URL
    "https://github.com/livekit/client-sdk-cpp/releases/download/v${LIVEKIT_VERSION}/livekit-sdk-windows-x64-${LIVEKIT_VERSION}.zip"
)

# 下载缓存目录（避免重复下载）
set(LIVEKIT_DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/extend/.cache")
set(LIVEKIT_ARCHIVE_FILE "${LIVEKIT_DOWNLOAD_DIR}/livekit-sdk-windows-x64-${LIVEKIT_VERSION}.zip")

# 标记文件，用于判断是否已经完成解压
set(LIVEKIT_STAMP_FILE "${LIVEKIT_SDK_DIR}/.livekit-${LIVEKIT_VERSION}.stamp")

# ---------- 下载与解压逻辑 ----------
if(NOT EXISTS "${LIVEKIT_STAMP_FILE}")
    message(STATUS "======================================================")
    message(STATUS "  LiveKit SDK v${LIVEKIT_VERSION} not found, downloading...")
    message(STATUS "  URL: ${LIVEKIT_DOWNLOAD_URL}")
    message(STATUS "======================================================")

    # 创建缓存目录
    file(MAKE_DIRECTORY "${LIVEKIT_DOWNLOAD_DIR}")

    # 下载（如果缓存不存在）
    if(NOT EXISTS "${LIVEKIT_ARCHIVE_FILE}")
        message(STATUS "Downloading LiveKit SDK v${LIVEKIT_VERSION} ...")
        file(DOWNLOAD
            "${LIVEKIT_DOWNLOAD_URL}"
            "${LIVEKIT_ARCHIVE_FILE}"
            SHOW_PROGRESS
            STATUS DOWNLOAD_STATUS
            TLS_VERIFY ON
        )
        list(GET DOWNLOAD_STATUS 0 DOWNLOAD_RESULT)
        list(GET DOWNLOAD_STATUS 1 DOWNLOAD_ERROR)
        if(NOT DOWNLOAD_RESULT EQUAL 0)
            file(REMOVE "${LIVEKIT_ARCHIVE_FILE}")
            message(FATAL_ERROR "Failed to download LiveKit SDK: ${DOWNLOAD_ERROR}\n"
                                "Please check your network or download manually:\n"
                                "  ${LIVEKIT_DOWNLOAD_URL}\n"
                                "and extract to: ${LIVEKIT_SDK_DIR}")
        endif()
        message(STATUS "Download complete!")
    else()
        message(STATUS "Using cached archive: ${LIVEKIT_ARCHIVE_FILE}")
    endif()

    # 清理旧的 SDK 目录（保留 debug 目录不动）
    if(EXISTS "${LIVEKIT_SDK_DIR}")
        message(STATUS "Removing old SDK directory: ${LIVEKIT_SDK_DIR}")
        file(REMOVE_RECURSE "${LIVEKIT_SDK_DIR}")
    endif()

    # 解压到临时目录
    set(LIVEKIT_EXTRACT_TEMP "${LIVEKIT_DOWNLOAD_DIR}/_extract_temp")
    if(EXISTS "${LIVEKIT_EXTRACT_TEMP}")
        file(REMOVE_RECURSE "${LIVEKIT_EXTRACT_TEMP}")
    endif()
    file(MAKE_DIRECTORY "${LIVEKIT_EXTRACT_TEMP}")

    message(STATUS "Extracting LiveKit SDK ...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf "${LIVEKIT_ARCHIVE_FILE}"
        WORKING_DIRECTORY "${LIVEKIT_EXTRACT_TEMP}"
        RESULT_VARIABLE EXTRACT_RESULT
    )
    if(NOT EXTRACT_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to extract LiveKit SDK!")
    endif()

    # 检测解压后的目录结构（可能有一层嵌套目录）
    file(GLOB EXTRACTED_CONTENTS "${LIVEKIT_EXTRACT_TEMP}/*")
    list(LENGTH EXTRACTED_CONTENTS CONTENT_COUNT)

    if(CONTENT_COUNT EQUAL 1 AND IS_DIRECTORY "${EXTRACTED_CONTENTS}")
        # 解压后只有一个子目录，移动其内容到目标位置
        set(EXTRACTED_SDK_DIR "${EXTRACTED_CONTENTS}")
    else()
        # 解压后直接就是 SDK 文件
        set(EXTRACTED_SDK_DIR "${LIVEKIT_EXTRACT_TEMP}")
    endif()

    # 移动到最终目标目录
    file(RENAME "${EXTRACTED_SDK_DIR}" "${LIVEKIT_SDK_DIR}")

    # 清理临时解压目录
    if(EXISTS "${LIVEKIT_EXTRACT_TEMP}")
        file(REMOVE_RECURSE "${LIVEKIT_EXTRACT_TEMP}")
    endif()

    # 写入 stamp 文件标记成功
    file(WRITE "${LIVEKIT_STAMP_FILE}" "LiveKit SDK v${LIVEKIT_VERSION} downloaded and extracted successfully.\n")

    message(STATUS "LiveKit SDK v${LIVEKIT_VERSION} installed at: ${LIVEKIT_SDK_DIR}")
else()
    message(STATUS "LiveKit SDK v${LIVEKIT_VERSION} already exists, skipping download.")
endif()

# ---------- 设置 SDK 路径变量 ----------
# v0.2.7 新版目录结构:
#   bin/   -> livekit.dll, livekit_ffi.dll
#   include/livekit/ -> 头文件
#   lib/   -> livekit.lib, livekit_ffi.dll.lib
#   lib/cmake/LiveKit/ -> CMake Config 文件

set(LIVEKIT_INCLUDE_DIR "${LIVEKIT_SDK_DIR}/include" CACHE PATH "LiveKit SDK include directory" FORCE)
set(LIVEKIT_LIB_DIR     "${LIVEKIT_SDK_DIR}/lib"     CACHE PATH "LiveKit SDK library directory" FORCE)
set(LIVEKIT_BIN_DIR     "${LIVEKIT_SDK_DIR}/bin"     CACHE PATH "LiveKit SDK binary directory" FORCE)

# 验证关键文件存在
foreach(_check_file
    "${LIVEKIT_INCLUDE_DIR}/livekit/livekit.h"
    "${LIVEKIT_LIB_DIR}/livekit.lib"
    "${LIVEKIT_LIB_DIR}/livekit_ffi.dll.lib"
)
    if(NOT EXISTS "${_check_file}")
        message(WARNING "LiveKit SDK file missing: ${_check_file}")
    endif()
endforeach()

# ---------- 创建 Imported Target ----------
# 创建 LiveKit::livekit imported target
if(NOT TARGET LiveKit::livekit)
    add_library(LiveKit::livekit SHARED IMPORTED)
    set_target_properties(LiveKit::livekit PROPERTIES
        IMPORTED_IMPLIB "${LIVEKIT_LIB_DIR}/livekit.lib"
        IMPORTED_LOCATION "${LIVEKIT_BIN_DIR}/livekit.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${LIVEKIT_INCLUDE_DIR}"
    )
endif()

# 创建 LiveKit::livekit_ffi imported target
if(NOT TARGET LiveKit::livekit_ffi)
    add_library(LiveKit::livekit_ffi SHARED IMPORTED)
    set_target_properties(LiveKit::livekit_ffi PROPERTIES
        IMPORTED_IMPLIB "${LIVEKIT_LIB_DIR}/livekit_ffi.dll.lib"
        IMPORTED_LOCATION "${LIVEKIT_BIN_DIR}/livekit_ffi.dll"
    )
endif()

# ---------- 辅助函数：复制 DLL 到构建输出目录 ----------
function(livekit_copy_dlls target_name)
    # 复制 livekit.dll
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LIVEKIT_BIN_DIR}/livekit.dll"
            $<TARGET_FILE_DIR:${target_name}>
        COMMENT "Copying livekit.dll to output directory..."
    )

    # 复制 livekit_ffi.dll
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LIVEKIT_BIN_DIR}/livekit_ffi.dll"
            $<TARGET_FILE_DIR:${target_name}>
        COMMENT "Copying livekit_ffi.dll to output directory..."
    )
endfunction()

message(STATUS "------------------------------------------------------")
message(STATUS "  LiveKit SDK v${LIVEKIT_VERSION} configured")
message(STATUS "  Include: ${LIVEKIT_INCLUDE_DIR}")
message(STATUS "  Lib:     ${LIVEKIT_LIB_DIR}")
message(STATUS "  Bin:     ${LIVEKIT_BIN_DIR}")
message(STATUS "------------------------------------------------------")
