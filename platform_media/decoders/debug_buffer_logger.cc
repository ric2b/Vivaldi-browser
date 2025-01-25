// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/decoders/debug_buffer_logger.h"

#include "media/base/decoder_buffer.h"

#ifndef NDEBUG
#include "base/files/file_util.h"
#include "base/uuid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#endif  // NDEBUG

// Uncomment to turn on content logging
//#define CONTENT_LOG_FOLDER FILE_PATH_LITERAL("D:\\logs")

namespace media {

DebugBufferLogger::DebugBufferLogger() {}

DebugBufferLogger::~DebugBufferLogger() {}

void DebugBufferLogger::Initialize(const std::string& stream_type) {
#if !defined(NDEBUG) && defined(CONTENT_LOG_FOLDER)
  log_directory_ = base::FilePath(CONTENT_LOG_FOLDER)
                       .Append(base::ASCIIToUTF16(stream_type) + L" - " +
                               base::ASCIIToUTF16(base::Uuid::GenerateRandomV4()));
  if (!CreateDirectory(log_directory_))
    log_directory_.clear();
#endif  // NDEBUG && CONTENT_LOG_FOLDERs
}

void DebugBufferLogger::Log(const DecoderBuffer& buffer) {
#if !defined(NDEBUG)
  if (log_directory_.empty())
    return;

  if (buffer.end_of_stream())
    return;

  base::WriteFile(
      log_directory_.AppendASCII(
          base::NumberToString(buffer.timestamp().InMilliseconds())),
      buffer);
#endif  // NDEBUG
}

}  // namespace media