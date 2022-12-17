// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_DECODERS_BUFFER_LOGGER_H_
#define PLATFORM_MEDIA_DECODERS_BUFFER_LOGGER_H_

#include "base/files/file_path.h"

namespace media {

class DecoderBuffer;

class DebugBufferLogger {
 public:
  DebugBufferLogger();
  ~DebugBufferLogger();

  void Initialize(const std::string& stream_type);

  void Log(const DecoderBuffer& buffer);

 private:
#ifndef NDEBUG
  base::FilePath log_directory_;
#endif  // NDEBUG
};
}  // namespace media

#endif  // PLATFORM_MEDIA_DECODERS_BUFFER_LOGGER_H_
