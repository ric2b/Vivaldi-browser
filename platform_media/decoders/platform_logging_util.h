// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//
// This file is an original work developed by Vivaldi Technologies AS.

#ifndef PLATFORM_MEDIA_DECODERS_PLATFORM_LOGGING_UTIL_H_
#define PLATFORM_MEDIA_DECODERS_PLATFORM_LOGGING_UTIL_H_

#include <string>

namespace media {

class VideoDecoderConfig;

std::string Loggable(const VideoDecoderConfig& config);

}  // namespace media

#endif  // PLATFORM_MEDIA_DECODERS_PLATFORM_LOGGING_UTIL_H_
