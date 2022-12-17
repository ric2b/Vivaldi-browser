// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//
// This file is an original work developed by Vivaldi Technologies AS.

#include "platform_media/decoders/platform_logging_util.h"

#include <iomanip>
#include <iostream>
#include <sstream>

#include "media/base/video_decoder_config.h"

namespace media {

std::string Loggable(const VideoDecoderConfig& config) {
  std::ostringstream s;
  s << "\n VideoDecoderConfig is Valid : "
    << (config.IsValidConfig() ? "true" : "false")
    << "\n Codec : " << GetCodecName(config.codec()) << "\n Alpha mode : "
    << (config.alpha_mode() != VideoDecoderConfig::AlphaMode::kIsOpaque
            ? "true"
            : "false")
    //<< "\n VideoPixelFormat : " << VideoPixelFormatToString(config.format())
    //<< "\n ColorSpace : " << config.color_space_info()
    << "\n VideoCodecProfile : " << GetProfileName(config.profile())
    << "\n Coded Size: [" << config.coded_size().width() << ","
    << config.coded_size().height() << "]"
    << "\n Visible Rect: [x: " << config.visible_rect().x()
    << ", y: " << config.visible_rect().y()
    << ", width: " << config.visible_rect().width()
    << ", height: " << config.visible_rect().height() << "]"
    << "\n Natural Size: [ width: " << config.natural_size().width()
    << ", height: " << config.natural_size().height() << "]"
    << "\n encrypted : " << (config.is_encrypted() ? "true" : "false")
    << "\n size of extra data : " << config.extra_data().size();

  if (!config.extra_data().empty() && config.extra_data().size() < 50) {
    s << "\n extra data : \n";
    size_t count = 0;
    for (auto const& data : config.extra_data()) {
      s << "0x" << std::uppercase << std::setfill('0') << std::setw(2)
        << std::hex << int(data) << ", ";
      count++;

      if (count % 8 == 0)
        s << "\n";
    }
  }

  return s.str();
}

}  // namespace media
