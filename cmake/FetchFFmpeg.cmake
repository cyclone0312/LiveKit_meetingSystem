# ==============================================================================
# FetchFFmpeg.cmake — 自动下载预编译 FFmpeg 并创建 Imported Targets
#
# 用法: include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchFFmpeg.cmake)
#
# 创建的 Imported Targets:
#   FFmpeg::avcodec   FFmpeg::avformat   FFmpeg::avutil
#   FFmpeg::swscale   FFmpeg::swresample
#
# 变量:
#   FFMPEG_INCLUDE_DIR  — 头文件目录
#   FFMPEG_BIN_DIR      — DLL 所在目录（Windows）
# ==============================================================================

include(FetchContent)

set(FFMPEG_VERSION "7.1" CACHE STRING "FFmpeg version to download")

if(WIN32)
    # Windows: 使用 gyan.dev 的 shared 预编译包
    set(FFMPEG_URL "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip")
    set(FFMPEG_ARCHIVE_NAME "ffmpeg-master-latest-win64-gpl-shared")
else()
    message(FATAL_ERROR "FetchFFmpeg: 当前仅支持 Windows 平台的预编译包。其他平台请手动安装 FFmpeg。")
endif()

# 检查是否已经存在本地预编译 FFmpeg
set(FFMPEG_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg")

if(EXISTS "${FFMPEG_LOCAL_DIR}/include/libavcodec/avcodec.h")
    message(STATUS "[FFmpeg] 使用本地预编译 FFmpeg: ${FFMPEG_LOCAL_DIR}")
    set(FFMPEG_ROOT "${FFMPEG_LOCAL_DIR}")
else()
    message(STATUS "[FFmpeg] 本地未找到，从网络下载预编译 FFmpeg...")
    FetchContent_Declare(ffmpeg
        URL ${FFMPEG_URL}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(ffmpeg)
    set(FFMPEG_ROOT "${ffmpeg_SOURCE_DIR}")
    message(STATUS "[FFmpeg] 已下载到: ${FFMPEG_ROOT}")
endif()

# 设置路径
set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include" CACHE PATH "FFmpeg include directory")
set(FFMPEG_LIB_DIR     "${FFMPEG_ROOT}/lib"     CACHE PATH "FFmpeg lib directory")
set(FFMPEG_BIN_DIR     "${FFMPEG_ROOT}/bin"     CACHE PATH "FFmpeg bin directory")

# 辅助宏: 创建一个 FFmpeg imported target
macro(ffmpeg_add_imported_lib target_name lib_name)
    if(NOT TARGET FFmpeg::${target_name})
        add_library(FFmpeg::${target_name} SHARED IMPORTED)

        if(WIN32)
            set_target_properties(FFmpeg::${target_name} PROPERTIES
                IMPORTED_IMPLIB   "${FFMPEG_LIB_DIR}/${lib_name}.lib"
                IMPORTED_LOCATION "${FFMPEG_BIN_DIR}/${lib_name}.dll"
                INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}"
            )
            # 有些包的 lib 文件名不一定带 .lib 后缀的格式
            if(NOT EXISTS "${FFMPEG_LIB_DIR}/${lib_name}.lib")
                # 尝试查找其他命名格式
                file(GLOB _ffmpeg_lib_files "${FFMPEG_LIB_DIR}/${lib_name}*")
                if(_ffmpeg_lib_files)
                    list(GET _ffmpeg_lib_files 0 _ffmpeg_lib_file)
                    set_target_properties(FFmpeg::${target_name} PROPERTIES
                        IMPORTED_IMPLIB "${_ffmpeg_lib_file}"
                    )
                endif()
            endif()
        endif()
    endif()
endmacro()

# 创建 Imported Targets
ffmpeg_add_imported_lib(avcodec    avcodec)
ffmpeg_add_imported_lib(avformat   avformat)
ffmpeg_add_imported_lib(avutil     avutil)
ffmpeg_add_imported_lib(swscale    swscale)
ffmpeg_add_imported_lib(swresample swresample)

# 辅助函数: 复制 FFmpeg DLL 到输出目录
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
