// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#include "platform_media/ffmpeg/ffviv_log.h"

#include "base/logging.h"

namespace {

logging::LogSeverity ConvertFFmpegLogLevelToSeverity(int ffmpeg_log_level) {
  if (ffmpeg_log_level <= AV_LOG_FATAL)
    return logging::LOGGING_FATAL;
  if (ffmpeg_log_level <= AV_LOG_ERROR)
    return logging::LOGGING_VERBOSE;
  if (ffmpeg_log_level <= AV_LOG_WARNING)
    return -2;
  if (ffmpeg_log_level <= AV_LOG_INFO)
    return -3;
  if (ffmpeg_log_level <= AV_LOG_VERBOSE)
    return -5;
  if (ffmpeg_log_level <= AV_LOG_DEBUG)
    return -7;
  // AV_LOG_TRACE
  return -9;
}

}  // namespace

extern "C" {

int ffviv_log_is_on(const char* file_path, int ffmpeg_log_level) {
  int verbosity =
      ::logging::GetVlogLevelHelper(file_path, strlen(file_path) + 1);
  return ConvertFFmpegLogLevelToSeverity(ffmpeg_log_level) >= -verbosity;
}

void ffviv_log(int ffmpeg_log_level,
               const char* file_path,
               int line_number,
               const char* function_name,
               const char* format,
               ...) {
  char message[200];
  va_list ap;
  va_start(ap, format);
  vsnprintf(message, sizeof message, format, ap);
  va_end(ap);

  // The trailing \n is redundant as Chromium loggings adds one on its own.
  size_t n = strlen(message);
  if (n != 0 && message[n - 1] == '\n') {
    message[n - 1] = '\0';
  }

  ::logging::LogMessage msg(file_path, line_number,
                            ConvertFFmpegLogLevelToSeverity(ffmpeg_log_level));
  if (function_name) {
    msg.stream() << function_name << ": ";
  }
  msg.stream() << message;
}
}
