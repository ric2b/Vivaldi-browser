// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef PLATFORM_MEDIA_FFMPEG_WMFAACDEC_H_
#define PLATFORM_MEDIA_FFMPEG_WMFAACDEC_H_

#ifdef __cplusplus
extern "C" {
#endif

// Chromium disables FFmpeg logging infrastructure at the compile time to avoid
// code bloat due to log strings and to squeeze max performance. In addition on
// Windows logging to stderr is not supported from the renderer process, one has
// to use base/logging.h So provide a minimal logging API that redirects to
// Chromium VLOG() with the given verbosity level.
typedef void (*FFWMF_LogFunction)(int verbosity_level,
                                  const char* file_path,
                                  int line_number,
                                  const char* function_name,
                                  const char* message);

// Global log data that should be initialized by the application when FFmpeg is
// initialized.
struct FFWMF_LogInfo {
  int max_verbosity;
  FFWMF_LogFunction log_function;
  const char* const file_path;
};

struct FFWMF_LogInfo* ffwmf_get_log_info();

#ifdef __cplusplus
}
#endif

#endif  // PLATFORM_MEDIA_FFMPEG_WMFAACDEC_H_
