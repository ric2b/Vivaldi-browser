// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef PLATFORM_MEDIA_FFMPEG_FFVIV_LOG_H_
#define PLATFORM_MEDIA_FFMPEG_FFVIV_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "third_party/ffmpeg/libavutil/log.h"

#if defined(OFFICIAL_BUILD)
#define FFVIV_OFFICIAL_BUILD 1
#else
#define FFVIV_OFFICIAL_BUILD 0
#endif

// Replace FFfmpeg logging with something that works with Chromium API and
// supports file-level --vmodule filtering. To make this work with all FFmpeg
// sources without patching them we generate at the build time  libavutil/log.h
// that includes this file and which is placed on the include list before
// libavutil/log.h, see :generate_redirected_log target in BUILD.gn.

#if defined(av_log)
#undef av_log
#endif

#define av_log(avcl, log_level, ...)                                        \
  do {                                                                      \
    /* Do not bloat an official build with debug logs. */                   \
    int _ffviv_log_level = log_level;                                       \
    if (!FFVIV_OFFICIAL_BUILD || _ffviv_log_level <= AV_LOG_ERROR) {        \
      if (ffviv_log_is_on(__FILE__, _ffviv_log_level)) {                    \
        /* Do not bloat an official build with function names. */           \
        ffviv_log(_ffviv_log_level, __FILE__, __LINE__,                     \
                  FFVIV_OFFICIAL_BUILD ? NULL : __FUNCTION__, __VA_ARGS__); \
      }                                                                     \
    }                                                                       \
  } while (0)

int ffviv_log_is_on(const char* file_path, int ffmpeg_log_level);

void ffviv_log(int ffmpeg_log_level,
               const char* file_path,
               int line_number,
               const char* function_name,
               const char* format,
               ...) av_printf_format(5, 6);

#ifdef __cplusplus
}
#endif

#endif  // PLATFORM_MEDIA_FFMPEG_FFVIV_LOG_H_
