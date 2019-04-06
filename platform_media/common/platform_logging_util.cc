// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//
// This file is an original work developed by Vivaldi Technologies AS.

#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"

#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"

#include <iomanip>
#include <iostream>

namespace media {

std::string Loggable(const PlatformVideoConfig & config) {
  std::ostringstream s;
  s << "\n PlatformVideoConfig is Valid : " << (config.is_valid() ? "true" : "false");
  return s.str();
}

std::string Loggable(const VideoDecoderConfig & config) {
  std::ostringstream s;
  s << "\n VideoDecoderConfig is Valid : " << (config.IsValidConfig() ? "true" : "false")
    << "\n Codec : " << GetCodecName(config.codec())
    << "\n VideoPixelFormat : " << VideoPixelFormatToString(config.format())
    << "\n ColorSpace : " << config.color_space()
    << "\n VideoCodecProfile : " << GetProfileName(config.profile())
    << "\n Coded Size: ["
    << config.coded_size().width()
    << "," << config.coded_size().height()
    << "]"
    << "\n Visible Rect: [x: "
    << config.visible_rect().x()
    << ", y: " << config.visible_rect().y()
    << ", width: " << config.visible_rect().width()
    << ", height: " << config.visible_rect().height()
    << "]"
    << "\n Natural Size: [ width: "
    << config.natural_size().width() << ", height: "
    << config.natural_size().height()
    << "]"
    << "\n encrypted : " << (config.is_encrypted() ? "true" : "false")
    << "\n size of extra data : " << config.extra_data().size();

  if(!config.extra_data().empty() && config.extra_data().size() < 50) {
      s << "\n extra data : \n";
    size_t count = 0;
    for(auto const& data: config.extra_data()) {
      s  << "0x" << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << int(data) << ", ";
      count++;

      if (count % 8 == 0)
          s << "\n";
    }
  }

  return s.str();
}

std::string Loggable(const AudioDecoderConfig & config) {
  std::ostringstream s;
  s << "\n AudioDecoderConfig is Valid : " << (config.IsValidConfig() ? "true" : "false")
    << "\n Codec : " << GetCodecName(config.codec())
    << "\n ChannelLayout : " << ChannelLayoutToString(config.channel_layout())
    << "\n SampleFormat : " << SampleFormatToString(config.sample_format())
    << "\n bytes_per_channel : " << config.bytes_per_channel()
    << "\n bytes_per_frame : " << config.bytes_per_frame()
    << "\n samples_per_second : " << config.samples_per_second()
    << "\n seek_preroll : " << config.seek_preroll().InMilliseconds() << "ms"
    << "\n codec_delay : " << config.codec_delay()
    << "\n encrypted : " << (config.is_encrypted() ? "true" : "false")
    << "\n size of extra data : " << config.extra_data().size();

  if(!config.extra_data().empty() && config.extra_data().size() < 50) {
      s << "\n extra data : ";
    size_t count = 0;
    for(auto const& data: config.extra_data()) {
      s  << "[" << count << "]:"<< int(data) << " ";
      count++;
    }
  }

  return s.str();
}

std::string Loggable(const PlatformAudioConfig & config) {
  std::ostringstream s;
  s << "\n PlatformAudioConfig is Valid : " << (config.is_valid() ? "true" : "false")
    << "\n SampleFormat : " << SampleFormatToString(config.format)
    << "\n channel_count : " << config.channel_count
    << "\n samples_per_second : " << config.samples_per_second;
  return s.str();
}

std::string LoggableMediaType(PlatformMediaDataType type) {
    switch (type) {
      case PlatformMediaDataType::PLATFORM_MEDIA_AUDIO: return "AUDIO";
      case PlatformMediaDataType::PLATFORM_MEDIA_VIDEO: return "VIDEO";
      default : return "UNKNOWN";
    }
}

}
