// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef PLATFORM_MEDIA_COMMON_PLATFORM_MEDIA_MOJOM_TRAITS_H_
#define PLATFORM_MEDIA_COMMON_PLATFORM_MEDIA_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

#include "platform_media/ipc_demuxer/platform_media.mojom-shared.h"
#include "platform_media/ipc_demuxer/platform_media_pipeline_types.h"

namespace mojo {

// Map Mojo types into platform_media_pipeine_types, see the documentation in
// the above enum_traits.h and struct_traits.h headers.

template <>
struct EnumTraits<platform_media::mojom::StreamType,
                  media::PlatformStreamType> {
  static_assert(
      static_cast<int32_t>(platform_media::mojom::StreamType::kAudio) ==
          static_cast<int>(media::PlatformStreamType::kAudio),
      "audio must match");
  static_assert(
      static_cast<int32_t>(platform_media::mojom::StreamType::kVideo) ==
          static_cast<int>(media::PlatformStreamType::kVideo),
      "video must match");
  static_assert(
      static_cast<int32_t>(platform_media::mojom::StreamType::kMaxValue) + 1 ==
          media::kPlatformStreamTypeCount,
      "size must match");

  static platform_media::mojom::StreamType ToMojom(
      media::PlatformStreamType input) {
    return static_cast<platform_media::mojom::StreamType>(
        static_cast<int>(input));
  }

  static bool FromMojom(platform_media::mojom::StreamType input,
                        media::PlatformStreamType* output) {
    *output =
        static_cast<media::PlatformStreamType>(static_cast<int32_t>(input));
    return true;
  }
};

template <>
class StructTraits<platform_media::mojom::TimeInfoDataView,
                   media::PlatformMediaTimeInfo> {
 public:
  using View = platform_media::mojom::TimeInfoDataView;
  using T = media::PlatformMediaTimeInfo;

  static base::TimeDelta duration(const T& t) { return t.duration; }
  static base::TimeDelta start_time(const T& t) { return t.start_time; }

  static bool Read(View data, T* output);
};

template <>
class StructTraits<platform_media::mojom::AudioConfigDataView,
                   media::PlatformAudioConfig> {
 public:
  using View = platform_media::mojom::AudioConfigDataView;
  using T = media::PlatformAudioConfig;

  static int32_t format(const T& t) { return t.format; }
  static int32_t channel_count(const T& t) { return t.channel_count; }
  static int32_t samples_per_second(const T& t) { return t.samples_per_second; }

  static bool Read(View data, T* output);
};

template <>
class StructTraits<platform_media::mojom::VideoPlaneConfigDataView,
                   media::PlatformVideoConfig::Plane> {
 public:
  using View = platform_media::mojom::VideoPlaneConfigDataView;
  using T = media::PlatformVideoConfig::Plane;

  static int32_t stride(const T& t) { return t.stride; }
  static int32_t offset(const T& t) { return t.offset; }
  static int32_t size(const T& t) { return t.size; }

  static bool Read(View data, T* output);
};

template <>
class StructTraits<platform_media::mojom::VideoConfigDataView,
                   media::PlatformVideoConfig> {
 public:
  using View = platform_media::mojom::VideoConfigDataView;
  using T = media::PlatformVideoConfig;

  static gfx::Size coded_size(const T& t) { return t.coded_size; }
  static const gfx::Rect& visible_rect(const T& t) { return t.visible_rect; }
  static gfx::Size natural_size(const T& t) { return t.natural_size; }
  static const T::PlaneArray& planes(const T& t) { return t.planes; }
  static int32_t rotation(const T& t) { return t.rotation; }

  static bool Read(View data, T* output);
};

}  // namespace mojo

#endif  // PLATFORM_MEDIA_COMMON_PLATFORM_MEDIA_MOJOM_TRAITS_H_
