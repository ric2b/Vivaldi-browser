// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "media/test/pipeline_integration_test_base.h"

#include "base/command_line.h"
#include "platform_media/test/test_pipeline_host.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/base/video_decoder_config.h"
#include "platform_media/renderer/decoders/ipc_demuxer.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace media {

#if defined(OS_WIN)
namespace {
DemuxerStream* GetStream(std::unique_ptr<Demuxer> & demuxer, DemuxerStream::Type type) {
    std::vector<DemuxerStream*> streams = demuxer->GetAllStreams();
    for (auto* stream : streams) {
      if (stream->type() == type)
        return stream;
    }
    return nullptr;
}
}
#endif  // defined(OS_WIN)

class PlatformMediaPipelineIntegrationTest
    : public testing::Test,
      public PipelineIntegrationTestBase {
 public:
  static bool IsEnabled() {
#if defined(OS_MACOSX)
    return true;
#elif defined(OS_WIN)
    if (base::win::GetVersion() >= base::win::VERSION_WIN7)
      return true;
#endif
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " Unsupported OS, skipping test";
    return false;
  }
};

#if defined(OS_MACOSX) || defined(OS_WIN)
TEST_F(PlatformMediaPipelineIntegrationTest, BasicPlayback) {
  if (!IsEnabled())
    return;

  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("bear.mp4", kHashed));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());

#if defined(OS_MACOSX)
  if (base::mac::IsAtLeastOS10_10()) {
    EXPECT_EQ("bd1d880e4934bf76c0bb34450cd0f173", GetVideoHash());
    EXPECT_EQ("-0.51,0.54,1.03,0.85,-0.08,-0.22,", GetAudioHash());
  } else {
    // On OS X 10.9, the expected hashes can be different, because our solution
    // doesn't necessarily process frames one by one, see AVFMediaDecoder.
    EXPECT_EQ("-1.38,-0.99,0.56,1.71,1.48,0.23,", GetAudioHash());
  }
#elif defined(OS_WIN)
  EXPECT_EQ("eb228dfe6882747111161156164dcab0", GetVideoHash());
  EXPECT_EQ("-0.52,0.26,0.16,0.24,-0.00,0.26,", GetAudioHash());
#endif
  EXPECT_TRUE(demuxer_->GetTimelineOffset().is_null());
}

TEST_F(PlatformMediaPipelineIntegrationTest, BasicPlayback_16x9_Aspect) {
  if (!IsEnabled())
    return;

  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("vivaldi-bear-320x240-16x9-aspect.mp4", kHashed));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());

#if defined(OS_MACOSX)
  if (base::mac::IsAtLeastOS10_10()) {
    EXPECT_EQ("e9a2e53ef2c16757962cc58d37de69e7", GetVideoHash());
    EXPECT_EQ("-3.66,-2.08,0.22,2.09,0.64,-0.90,", GetAudioHash());
  } else {
    // On OS X, the expected hashes can be different, because our solution
    // doesn't necessarily process frames one by one, see AVFMediaDecoder.
    EXPECT_EQ("-1.81,-0.36,-0.20,0.84,-0.52,-1.11,", GetAudioHash());
  }
#elif defined(OS_WIN)
  EXPECT_EQ("e9a2e53ef2c16757962cc58d37de69e7", GetVideoHash());
  EXPECT_EQ("-3.60,-1.82,0.28,1.90,0.34,-1.09,", GetAudioHash());
#endif
}

TEST_F(PlatformMediaPipelineIntegrationTest, BasicPlayback_VideoOnly) {
  if (!IsEnabled())
    return;

  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("bear_silent.mp4", kHashed));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());

#if defined(OS_MACOSX)
  if (base::mac::IsAtLeastOS10_10()) {
    EXPECT_EQ("e7832270a91e8de7945b5724eec2cbcb", GetVideoHash());
  }
  // On OS X, the expected hashes can be different, because our solution
  // doesn't necessarily process frames one by one, see AVFMediaDecoder.
#elif defined(OS_WIN)
  EXPECT_EQ("eb228dfe6882747111161156164dcab0", GetVideoHash());
#endif
}

TEST_F(PlatformMediaPipelineIntegrationTest, BasicPlayback_M4A) {
  if (!IsEnabled())
    return;

  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("sfx.m4a", kHashed));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());

#if defined(OS_MACOSX)
  if (base::mac::IsAtLeastOS10_10()) {
    EXPECT_EQ("-5.29,-5.47,-5.05,-4.33,-2.99,-3.79,", GetAudioHash());
  } else {
    // On OS X, the expected hashes can be different, because our solution
    // doesn't necessarily process frames one by one, see AVFMediaDecoder.
    EXPECT_EQ("-4.97,-3.80,-3.26,-3.75,-4.90,-5.83,", GetAudioHash());
  }
#elif defined(OS_WIN)
  EXPECT_EQ("0.46,1.72,4.26,4.57,3.39,1.54,", GetAudioHash());
#endif
}

TEST_F(PlatformMediaPipelineIntegrationTest, SeekWhilePaused) {
  if (!IsEnabled())
    return;

  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("bear.mp4"));

  base::TimeDelta duration(pipeline_->GetMediaDuration());
  base::TimeDelta start_seek_time(duration / 4);
  base::TimeDelta seek_time(duration * 3 / 4);

  Play();
  ASSERT_TRUE(WaitUntilCurrentTimeIsAfter(start_seek_time));
  Pause();
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_EQ(pipeline_->GetMediaTime(), seek_time);
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());

  // Make sure seeking after reaching the end works as expected.
  Pause();
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_EQ(pipeline_->GetMediaTime(), seek_time);
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

TEST_F(PlatformMediaPipelineIntegrationTest, SeekWhilePlaying) {
  if (!IsEnabled())
    return;

  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("bear.mp4"));

  base::TimeDelta duration(pipeline_->GetMediaDuration());
  base::TimeDelta start_seek_time(duration / 4);
  base::TimeDelta seek_time(duration * 3 / 4);

  Play();
  ASSERT_TRUE(WaitUntilCurrentTimeIsAfter(start_seek_time));
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_GE(pipeline_->GetMediaTime(), seek_time);
  ASSERT_TRUE(WaitUntilOnEnded());

  // Make sure seeking after reaching the end works as expected.
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_GE(pipeline_->GetMediaTime(), seek_time);
  ASSERT_TRUE(WaitUntilOnEnded());
}

TEST_F(PlatformMediaPipelineIntegrationTest, Seek_VideoOnly) {
  if (!IsEnabled())
    return;

  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("bear_silent.mp4", kHashed));

  Play();
  ASSERT_TRUE(Seek(pipeline_->GetMediaDuration() / 2));

  ASSERT_TRUE(WaitUntilOnEnded());
}

TEST_F(PlatformMediaPipelineIntegrationTest, PlayInLoop) {
  if (!IsEnabled())
    return;

  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("bear.mp4"));

  const base::TimeDelta duration = pipeline_->GetMediaDuration();
  const base::TimeDelta play_time = duration / 4;

  Play();
  ASSERT_TRUE(WaitUntilCurrentTimeIsAfter(play_time));
  ASSERT_TRUE(Seek(duration));
  ASSERT_TRUE(WaitUntilOnEnded());

  ASSERT_TRUE(Seek(base::TimeDelta()));
  ASSERT_LT(pipeline_->GetMediaTime(), play_time);
  ASSERT_TRUE(WaitUntilCurrentTimeIsAfter(play_time));
}

TEST_F(PlatformMediaPipelineIntegrationTest, TruncatedMedia) {
  if (!IsEnabled())
    return;

  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("vivaldi-bear_truncated.mp4"));

  Play();
  WaitUntilCurrentTimeIsAfter(base::TimeDelta::FromMicroseconds(1066666));
  ASSERT_TRUE(ended_ || pipeline_status_ != PipelineStatus::PIPELINE_OK);
}

TEST_F(PlatformMediaPipelineIntegrationTest, DecodingError) {
  if (!IsEnabled())
    return;

#if defined(OS_MACOSX)
  // AVPlayer hides the error.
  if (base::mac::IsOS10_9())
    return;
#endif

  // TODO(wdzierzanowski): WMFMediaPipeline (Windows) doesn't detect the error?
  // (DNA-30324).
#if !defined(OS_WIN)
  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("bear_corrupt.mp4"));
  Play();
  EXPECT_EQ(PipelineStatus::PIPELINE_ERROR_DECODE, WaitUntilEndedOrError());
#endif
}

TEST_F(PlatformMediaPipelineIntegrationTest, Rotated_Metadata_0) {
  if (!IsEnabled())
    return;

  // This is known not to work on Windows systems older than 8.
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;
#endif

  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("bear_rotate_0.mp4"));
  ASSERT_EQ(VideoRotation::VIDEO_ROTATION_0, metadata_.video_decoder_config.video_rotation());
}

TEST_F(PlatformMediaPipelineIntegrationTest, Rotated_Metadata_90) {
  if (!IsEnabled())
    return;

  // This is known not to work on Windows systems older than 8.
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;
#endif

  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("bear_rotate_90.mp4"));
  ASSERT_EQ(VideoRotation::VIDEO_ROTATION_90, metadata_.video_decoder_config.video_rotation());
}

TEST_F(PlatformMediaPipelineIntegrationTest, Rotated_Metadata_180) {
  if (!IsEnabled())
    return;

  // This is known not to work on Windows systems older than 8.
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;
#endif

  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("bear_rotate_180.mp4"));
  ASSERT_EQ(VideoRotation::VIDEO_ROTATION_180, metadata_.video_decoder_config.video_rotation());
}

TEST_F(PlatformMediaPipelineIntegrationTest, Rotated_Metadata_270) {
  if (!IsEnabled())
    return;

  // This is known not to work on Windows systems older than 8.
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;
#endif

  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("bear_rotate_270.mp4"));
  ASSERT_EQ(VideoRotation::VIDEO_ROTATION_270, metadata_.video_decoder_config.video_rotation());
}

// Configuration change happens only on Windows.
#if defined(OS_WIN)
TEST_F(PlatformMediaPipelineIntegrationTest, AudioConfigChange) {
  if (!IsEnabled())
    return;

  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("vivaldi-config_change_audio.mp4"));

  Play();

  AudioDecoderConfig audio_config =
      GetStream(demuxer_, DemuxerStream::AUDIO)->audio_decoder_config();

  EXPECT_EQ(audio_config.samples_per_second(), 24000);

  ASSERT_TRUE(WaitUntilOnEnded());

  audio_config =
      GetStream(demuxer_, DemuxerStream::AUDIO)->audio_decoder_config();
  EXPECT_EQ(audio_config.samples_per_second(), 48000);
}

TEST_F(PlatformMediaPipelineIntegrationTest, VideoConfigChange) {
  if (!IsEnabled())
    return;

  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("vivaldi-config_change_video.mp4"));

  Play();

  VideoDecoderConfig video_config =
      GetStream(demuxer_, DemuxerStream::VIDEO)->video_decoder_config();
  EXPECT_EQ(video_config.coded_size().height(), 270);

  ASSERT_TRUE(WaitUntilOnEnded());

  video_config =
      GetStream(demuxer_, DemuxerStream::VIDEO)->video_decoder_config();
  EXPECT_EQ(video_config.coded_size().height(), 272);
}
#endif  // defined(OS_WIN)

TEST_F(PlatformMediaPipelineIntegrationTest, BasicPlaybackPositiveStartTime) {

  ASSERT_EQ(PipelineStatus::PIPELINE_OK, Start("vivaldi-nonzero-start-time.mp4"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  ASSERT_EQ(base::TimeDelta::FromMicroseconds(390000),
            demuxer_->GetStartTime());
}

#endif  // defined(OS_MACOSX) || defined(OS_WIN)

}  // namespace media
