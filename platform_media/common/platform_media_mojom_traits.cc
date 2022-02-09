// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "platform_media/common/platform_media_mojom_traits.h"

#include "platform_media/common/platform_media.mojom.h"

namespace mojo {

// static
bool StructTraits<platform_media::mojom::TimeInfoDataView,
                  media::PlatformMediaTimeInfo>::Read(View data, T* output) {
  return data.ReadDuration(&output->duration) &&
         data.ReadStartTime(&output->start_time);
}

// static
bool StructTraits<platform_media::mojom::AudioConfigDataView,
                  media::PlatformAudioConfig>::Read(View data, T* output) {
  if (data.format() < 0 ||
      data.format() > media::SampleFormat::kSampleFormatMax ||
      data.channel_count() < 0 || data.samples_per_second() < 0) {
    return false;
  }
  output->format = static_cast<media::SampleFormat>(data.format());
  output->channel_count = data.channel_count();
  output->samples_per_second = data.samples_per_second();
  return true;
}

// static
bool StructTraits<platform_media::mojom::VideoPlaneConfigDataView,
                  media::PlatformVideoConfig::Plane>::Read(View data,
                                                           T* output) {
  if (data.stride() < 0 || data.offset() < 0 || data.size() < 0)
    return false;
  output->stride = data.stride();
  output->offset = data.offset();
  output->size = data.size();
  return true;
}

// static
bool StructTraits<platform_media::mojom::VideoConfigDataView,
                  media::PlatformVideoConfig>::Read(View data, T* output) {
  if (!data.ReadCodedSize(&output->coded_size))
    return false;
  if (!data.ReadVisibleRect(&output->visible_rect))
    return false;
  if (!data.ReadNaturalSize(&output->natural_size))
    return false;
  if (!data.ReadPlanes(&output->planes))
    return false;
  if (data.rotation() < 0 || data.rotation() > media::VIDEO_ROTATION_MAX)
    return false;
  output->rotation = static_cast<media::VideoRotation>(data.rotation());
  return true;
}

}  // namespace mojo
