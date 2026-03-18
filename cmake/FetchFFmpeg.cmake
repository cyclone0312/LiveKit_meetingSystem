# ==============================================================================
# FetchFFmpeg.cmake — 配置 FFmpeg 并创建 Imported Targets
#
# 用法: include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchFFmpeg.cmake)
#
# 平台策略:
#   - Windows: 优先使用 third_party/ffmpeg，本地不存在时自动下载预编译包
#   - Linux/macOS: 使用系统已安装的 FFmpeg 开发包
#
# 创建的 Imported Targets:
#   FFmpeg::avcodec   FFmpeg::avformat   FFmpeg::avutil
#   FFmpeg::swscale   FFmpeg::swresample
#
# 变量:
#   FFMPEG_INCLUDE_DIR  — 头文件目录
#   FFMPEG_BIN_DIR      — 运行库目录（Windows 为 bin；类 Unix 为 lib 目录）
# ==============================================================================

include(FetchContent)

set(FFMPEG_VERSION "7.1" CACHE STRING "FFmpeg version to download")

set(FFMPEG_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg")
set(FFMPEG_ROOT "")
set(FFMPEG_INCLUDE_DIR "")
set(FFMPEG_LIB_DIR "")
set(FFMPEG_BIN_DIR "")

macro(ffmpeg_fail message_text)
    message(FATAL_ERROR "FetchFFmpeg: ${message_text}")
endmacro()

function(ffmpeg_add_imported_lib target_name)
    set(options)
    set(oneValueArgs LIB_PATH)
    cmake_parse_arguments(FFMPEG_LIB "${options}" "${oneValueArgs}" "" ${ARGN})

    if(NOT FFMPEG_LIB_LIB_PATH)
        ffmpeg_fail("未提供 ${target_name} 的库路径")
    endif()

    if(NOT TARGET FFmpeg::${target_name})
        add_library(FFmpeg::${target_name} UNKNOWN IMPORTED)
        set_target_properties(FFmpeg::${target_name} PROPERTIES
            IMPORTED_LOCATION "${FFMPEG_LIB_LIB_PATH}"
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}"
        )
    endif()
endfunction()

if(EXISTS "${FFMPEG_LOCAL_DIR}/include/libavcodec/avcodec.h")
    message(STATUS "[FFmpeg] 使用本地预编译 FFmpeg: ${FFMPEG_LOCAL_DIR}")
    set(FFMPEG_ROOT "${FFMPEG_LOCAL_DIR}")
    set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
    set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
    if(WIN32)
        set(FFMPEG_BIN_DIR "${FFMPEG_ROOT}/bin")
    else()
        set(FFMPEG_BIN_DIR "${FFMPEG_LIB_DIR}")
    endif()

    if(WIN32)
        function(ffmpeg_add_windows_imported_lib target_name lib_name)
            if(NOT TARGET FFmpeg::${target_name})
                add_library(FFmpeg::${target_name} SHARED IMPORTED)
                set(_implib "${FFMPEG_LIB_DIR}/${lib_name}.lib")
                set(_dll    "${FFMPEG_BIN_DIR}/${lib_name}.dll")

                if(NOT EXISTS "${_implib}")
                    file(GLOB _ffmpeg_implibs "${FFMPEG_LIB_DIR}/${lib_name}*")
                    list(LENGTH _ffmpeg_implibs _implib_count)
                    if(_implib_count GREATER 0)
                        list(GET _ffmpeg_implibs 0 _implib)
                    endif()
                endif()

                if(NOT EXISTS "${_dll}")
                    file(GLOB _ffmpeg_dlls "${FFMPEG_BIN_DIR}/${lib_name}*.dll")
                    list(LENGTH _ffmpeg_dlls _dll_count)
                    if(_dll_count GREATER 0)
                        list(GET _ffmpeg_dlls 0 _dll)
                    endif()
                endif()

                if(NOT EXISTS "${_implib}" OR NOT EXISTS "${_dll}")
                    ffmpeg_fail("本地 FFmpeg 缺少 ${lib_name} 的 Windows 运行库")
                endif()

                set_target_properties(FFmpeg::${target_name} PROPERTIES
                    IMPORTED_IMPLIB "${_implib}"
                    IMPORTED_LOCATION "${_dll}"
                    INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}"
                )
            endif()
        endfunction()

        ffmpeg_add_windows_imported_lib(avcodec avcodec)
        ffmpeg_add_windows_imported_lib(avformat avformat)
        ffmpeg_add_windows_imported_lib(avutil avutil)
        ffmpeg_add_windows_imported_lib(swscale swscale)
        ffmpeg_add_windows_imported_lib(swresample swresample)
    else()
        ffmpeg_add_imported_lib(avcodec LIB_PATH "${FFMPEG_LIB_DIR}/libavcodec${CMAKE_SHARED_LIBRARY_SUFFIX}")
        ffmpeg_add_imported_lib(avformat LIB_PATH "${FFMPEG_LIB_DIR}/libavformat${CMAKE_SHARED_LIBRARY_SUFFIX}")
        ffmpeg_add_imported_lib(avutil LIB_PATH "${FFMPEG_LIB_DIR}/libavutil${CMAKE_SHARED_LIBRARY_SUFFIX}")
        ffmpeg_add_imported_lib(swscale LIB_PATH "${FFMPEG_LIB_DIR}/libswscale${CMAKE_SHARED_LIBRARY_SUFFIX}")
        ffmpeg_add_imported_lib(swresample LIB_PATH "${FFMPEG_LIB_DIR}/libswresample${CMAKE_SHARED_LIBRARY_SUFFIX}")
    endif()
elseif(WIN32)
    set(FFMPEG_URL "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip")
    message(STATUS "[FFmpeg] 本地未找到，从网络下载预编译 FFmpeg...")
    FetchContent_Declare(ffmpeg
        URL ${FFMPEG_URL}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(ffmpeg)

    set(FFMPEG_ROOT "${ffmpeg_SOURCE_DIR}")
    set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
    set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
    set(FFMPEG_BIN_DIR "${FFMPEG_ROOT}/bin")

    if(NOT EXISTS "${FFMPEG_INCLUDE_DIR}/libavcodec/avcodec.h")
        ffmpeg_fail("下载的 Windows FFmpeg 包不完整: ${FFMPEG_ROOT}")
    endif()

    function(ffmpeg_add_windows_imported_lib target_name lib_name)
        if(NOT TARGET FFmpeg::${target_name})
            add_library(FFmpeg::${target_name} SHARED IMPORTED)
            set(_implib "${FFMPEG_LIB_DIR}/${lib_name}.lib")
            set(_dll    "${FFMPEG_BIN_DIR}/${lib_name}.dll")

            if(NOT EXISTS "${_implib}")
                file(GLOB _ffmpeg_implibs "${FFMPEG_LIB_DIR}/${lib_name}*")
                list(LENGTH _ffmpeg_implibs _implib_count)
                if(_implib_count GREATER 0)
                    list(GET _ffmpeg_implibs 0 _implib)
                endif()
            endif()

            if(NOT EXISTS "${_dll}")
                file(GLOB _ffmpeg_dlls "${FFMPEG_BIN_DIR}/${lib_name}*.dll")
                list(LENGTH _ffmpeg_dlls _dll_count)
                if(_dll_count GREATER 0)
                    list(GET _ffmpeg_dlls 0 _dll)
                endif()
            endif()

            if(NOT EXISTS "${_implib}" OR NOT EXISTS "${_dll}")
                ffmpeg_fail("下载的 FFmpeg 缺少 ${lib_name} 的 Windows 运行库")
            endif()

            set_target_properties(FFmpeg::${target_name} PROPERTIES
                IMPORTED_IMPLIB "${_implib}"
                IMPORTED_LOCATION "${_dll}"
                INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}"
            )
        endif()
    endfunction()

    ffmpeg_add_windows_imported_lib(avcodec avcodec)
    ffmpeg_add_windows_imported_lib(avformat avformat)
    ffmpeg_add_windows_imported_lib(avutil avutil)
    ffmpeg_add_windows_imported_lib(swscale swscale)
    ffmpeg_add_windows_imported_lib(swresample swresample)
else()
    include(FindPkgConfig)

    if(PkgConfig_FOUND)
        pkg_check_modules(PC_FFMPEG_AVCODEC QUIET libavcodec)
        pkg_check_modules(PC_FFMPEG_AVFORMAT QUIET libavformat)
        pkg_check_modules(PC_FFMPEG_AVUTIL QUIET libavutil)
        pkg_check_modules(PC_FFMPEG_SWSCALE QUIET libswscale)
        pkg_check_modules(PC_FFMPEG_SWRESAMPLE QUIET libswresample)
    endif()

    find_path(FFMPEG_INCLUDE_DIR
        NAMES libavcodec/avcodec.h
        HINTS
            ${PC_FFMPEG_AVCODEC_INCLUDE_DIRS}
            ${PC_FFMPEG_AVFORMAT_INCLUDE_DIRS}
            ${PC_FFMPEG_AVUTIL_INCLUDE_DIRS}
            ${PC_FFMPEG_SWSCALE_INCLUDE_DIRS}
            ${PC_FFMPEG_SWRESAMPLE_INCLUDE_DIRS}
    )

    find_library(FFMPEG_AVCODEC_LIBRARY
        NAMES avcodec
        HINTS ${PC_FFMPEG_AVCODEC_LIBRARY_DIRS}
    )
    find_library(FFMPEG_AVFORMAT_LIBRARY
        NAMES avformat
        HINTS ${PC_FFMPEG_AVFORMAT_LIBRARY_DIRS}
    )
    find_library(FFMPEG_AVUTIL_LIBRARY
        NAMES avutil
        HINTS ${PC_FFMPEG_AVUTIL_LIBRARY_DIRS}
    )
    find_library(FFMPEG_SWSCALE_LIBRARY
        NAMES swscale
        HINTS ${PC_FFMPEG_SWSCALE_LIBRARY_DIRS}
    )
    find_library(FFMPEG_SWRESAMPLE_LIBRARY
        NAMES swresample
        HINTS ${PC_FFMPEG_SWRESAMPLE_LIBRARY_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(FFmpeg REQUIRED_VARS
        FFMPEG_INCLUDE_DIR
        FFMPEG_AVCODEC_LIBRARY
        FFMPEG_AVFORMAT_LIBRARY
        FFMPEG_AVUTIL_LIBRARY
        FFMPEG_SWSCALE_LIBRARY
        FFMPEG_SWRESAMPLE_LIBRARY
    )

    get_filename_component(FFMPEG_LIB_DIR "${FFMPEG_AVCODEC_LIBRARY}" DIRECTORY)
    set(FFMPEG_BIN_DIR "${FFMPEG_LIB_DIR}")

    ffmpeg_add_imported_lib(avcodec    LIB_PATH "${FFMPEG_AVCODEC_LIBRARY}")
    ffmpeg_add_imported_lib(avformat   LIB_PATH "${FFMPEG_AVFORMAT_LIBRARY}")
    ffmpeg_add_imported_lib(avutil     LIB_PATH "${FFMPEG_AVUTIL_LIBRARY}")
    ffmpeg_add_imported_lib(swscale    LIB_PATH "${FFMPEG_SWSCALE_LIBRARY}")
    ffmpeg_add_imported_lib(swresample LIB_PATH "${FFMPEG_SWRESAMPLE_LIBRARY}")
endif()

set(FFMPEG_INCLUDE_DIR "${FFMPEG_INCLUDE_DIR}" CACHE PATH "FFmpeg include directory")
set(FFMPEG_LIB_DIR "${FFMPEG_LIB_DIR}" CACHE PATH "FFmpeg library directory")
set(FFMPEG_BIN_DIR "${FFMPEG_BIN_DIR}" CACHE PATH "FFmpeg runtime directory")

message(STATUS "------------------------------------------------------")
message(STATUS "  FFmpeg")
message(STATUS "  Include: ${FFMPEG_INCLUDE_DIR}")
message(STATUS "  Library: ${FFMPEG_LIB_DIR}")
message(STATUS "  Bin:     ${FFMPEG_BIN_DIR}")
message(STATUS "------------------------------------------------------")

function(ffmpeg_copy_runtime_libs target)
    if(WIN32 AND EXISTS "${FFMPEG_BIN_DIR}")
        file(GLOB FFMPEG_DLLS "${FFMPEG_BIN_DIR}/*.dll")
        foreach(dll ${FFMPEG_DLLS})
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" $<TARGET_FILE_DIR:${target}>
                COMMENT "Copying FFmpeg DLL: ${dll}"
            )
        endforeach()
    endif()
endfunction()
