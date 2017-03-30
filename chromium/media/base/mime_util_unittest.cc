// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "media/base/mime_util.h"
#include "media/media_features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace media {

TEST(MimeUtilTest, CommonMediaMimeType) {
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/webm"));
  EXPECT_TRUE(IsSupportedMediaMimeType("video/webm"));

  EXPECT_TRUE(IsSupportedMediaMimeType("audio/wav"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/x-wav"));

  EXPECT_TRUE(IsSupportedMediaMimeType("audio/ogg"));
  EXPECT_TRUE(IsSupportedMediaMimeType("application/ogg"));
#if defined(OS_ANDROID)
  EXPECT_FALSE(IsSupportedMediaMimeType("video/ogg"));
#else
  EXPECT_TRUE(IsSupportedMediaMimeType("video/ogg"));
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
  // HLS is supported on Android API level 14 and higher and Chrome supports
  // API levels 15 and higher, so these are expected to be supported.
  bool kHlsSupported = true;
#else
  bool kHlsSupported = false;
#endif

  EXPECT_EQ(kHlsSupported, IsSupportedMediaMimeType("application/x-mpegurl"));
  EXPECT_EQ(kHlsSupported, IsSupportedMediaMimeType("Application/X-MPEGURL"));
  EXPECT_EQ(kHlsSupported, IsSupportedMediaMimeType(
      "application/vnd.apple.mpegurl"));

#if !defined(USE_SYSTEM_PROPRIETARY_CODECS)
#if defined(USE_PROPRIETARY_CODECS)
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/mp4"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/x-m4a"));
  EXPECT_TRUE(IsSupportedMediaMimeType("video/mp4"));
  EXPECT_TRUE(IsSupportedMediaMimeType("video/x-m4v"));

  EXPECT_TRUE(IsSupportedMediaMimeType("audio/mp3"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/x-mp3"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/mpeg"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/aac"));

#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
  EXPECT_TRUE(IsSupportedMediaMimeType("video/mp2t"));
#else
  EXPECT_FALSE(IsSupportedMediaMimeType("video/mp2t"));
#endif
#else
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/mp4"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/x-m4a"));
  EXPECT_FALSE(IsSupportedMediaMimeType("video/mp4"));
  EXPECT_FALSE(IsSupportedMediaMimeType("video/x-m4v"));

  EXPECT_FALSE(IsSupportedMediaMimeType("audio/mp3"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/x-mp3"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/mpeg"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/aac"));
#endif  // !defined(OS_LINUX) && defined(USE_PROPRIETARY_CODECS)
#endif  // !defined(USE_SYSTEM_PROPRIETARY_CODECS)
  EXPECT_FALSE(IsSupportedMediaMimeType("video/mp3"));

  EXPECT_FALSE(IsSupportedMediaMimeType("video/unknown"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/unknown"));
  EXPECT_FALSE(IsSupportedMediaMimeType("unknown/unknown"));
}

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
TEST(MimeUtilTest, CommonMediaMimeTypeSystemCodecs) {
  bool proprietary_audio_supported = false;
  bool proprietary_video_supported = false;
#if defined(OS_MACOSX)
  proprietary_audio_supported = true;
  proprietary_video_supported = base::mac::IsOSMavericksOrLater();
#elif defined(OS_WIN)
  proprietary_audio_supported =
      base::win::GetVersion() >= base::win::VERSION_WIN7;
  proprietary_video_supported = proprietary_audio_supported;
#endif

#define EXPECT_AUDIO_SUPPORT(mime_type)             \
  EXPECT_TRUE(IsSupportedMediaMimeType(mime_type) ^ \
              !proprietary_audio_supported)
#define EXPECT_VIDEO_SUPPORT(mime_type)             \
  EXPECT_TRUE(IsSupportedMediaMimeType(mime_type) ^ \
              !proprietary_video_supported)

  EXPECT_AUDIO_SUPPORT("audio/mp4");
  EXPECT_AUDIO_SUPPORT("audio/x-m4a");
  EXPECT_VIDEO_SUPPORT("video/mp4");
  EXPECT_VIDEO_SUPPORT("video/x-m4v");

  EXPECT_AUDIO_SUPPORT("audio/mp3");
  EXPECT_AUDIO_SUPPORT("audio/x-mp3");
  EXPECT_AUDIO_SUPPORT("audio/mpeg");
  EXPECT_AUDIO_SUPPORT("audio/aac");
}
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)

// Note: codecs should only be a list of 2 or fewer; hence the restriction of
// results' length to 2.
TEST(MimeUtilTest, ParseCodecString) {
  const struct {
    const char* const original;
    size_t expected_size;
    const char* const results[2];
  } tests[] = {
    { "\"bogus\"",                  1, { "bogus" }            },
    { "0",                          1, { "0" }                },
    { "avc1.42E01E, mp4a.40.2",     2, { "avc1",   "mp4a" }   },
    { "\"mp4v.20.240, mp4a.40.2\"", 2, { "mp4v",   "mp4a" }   },
    { "mp4v.20.8, samr",            2, { "mp4v",   "samr" }   },
    { "\"theora, vorbis\"",         2, { "theora", "vorbis" } },
    { "",                           0, { }                    },
    { "\"\"",                       0, { }                    },
    { "\"   \"",                    0, { }                    },
    { ",",                          2, { "", "" }             },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::vector<std::string> codecs_out;
    ParseCodecString(tests[i].original, &codecs_out, true);
    ASSERT_EQ(tests[i].expected_size, codecs_out.size());
    for (size_t j = 0; j < tests[i].expected_size; ++j)
      EXPECT_EQ(tests[i].results[j], codecs_out[j]);
  }

  // Test without stripping the codec type.
  std::vector<std::string> codecs_out;
  ParseCodecString("avc1.42E01E, mp4a.40.2", &codecs_out, false);
  ASSERT_EQ(2u, codecs_out.size());
  EXPECT_EQ("avc1.42E01E", codecs_out[0]);
  EXPECT_EQ("mp4a.40.2", codecs_out[1]);
}

}  // namespace media
