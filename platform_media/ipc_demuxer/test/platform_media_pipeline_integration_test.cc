// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "media/test/pipeline_integration_test_base.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/file_data_source.h"
#include "media/test/test_media_source.h"

#include "base/vivaldi_paths.h"
#include "platform_media/ipc_demuxer/renderer/ipc_demuxer.h"
#include "platform_media/ipc_demuxer/renderer/ipc_factory.h"
#include "platform_media/ipc_demuxer/test/ipc_pipeline_test_setup.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace media {

namespace {

const base::FilePath::CharType kPlatformMediaTestDataPath[] =
    FILE_PATH_LITERAL("platform_media");

base::FilePath GetPlatformMediaTestDataPath() {
  return base::FilePath(kPlatformMediaTestDataPath);
}

// IPCDemuxer expects that the pipeline host is already initialized when
// Chromium calls its Initialize from Demuxer interface using StartIPC() call.
// This subclass overrides Initialize to call StartIPC() first as this provides
// a convinient place to perform an asynchronous init.
class TestIPCDemuxer : public IPCDemuxer {
 public:
  TestIPCDemuxer(DataSource* data_source,
                 scoped_refptr<base::SequencedTaskRunner> media_task_runner,
                 std::string mime_type,
                 MediaLog* media_log)
      : IPCDemuxer(std::move(media_task_runner), media_log),
        data_source_(data_source),
        mime_type_(std::move(mime_type)) {}

  void Initialize(DemuxerHost* host,
                  PipelineStatusCallback status_cb) override {
    DataSource* data_source = data_source_;
    data_source_ = nullptr;

    // Unretained() is safe as the callback can only be called before the parent
    // class destructor is called.
    StartIPC(
        data_source, std::move(mime_type_),
        base::BindOnce(&TestIPCDemuxer::OnHostInitialized,
                       base::Unretained(this), host, std::move(status_cb)));
  }

  void OnHostInitialized(DemuxerHost* host,
                         PipelineStatusCallback status_cb,
                         bool success) {
    if (!success) {
      std::move(status_cb).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
      return;
    }
    IPCDemuxer::Initialize(host, std::move(status_cb));
  }

 private:
  DataSource* data_source_ = nullptr;
  std::string mime_type_;
};

}  // namespace

base::FilePath GetVivaldiTestDataFilePath(const std::string& name) {
  base::FilePath file_path;
  CHECK(base::PathService::Get(vivaldi::DIR_VIVALDI_TEST_DATA, &file_path));
  return file_path.Append(GetPlatformMediaTestDataPath()).AppendASCII(name);
}

class PlatformMediaMockMediaSource : public TestMediaSource {
 public:
  PlatformMediaMockMediaSource(const std::string& filename,
                               const std::string& mimetype,
                               size_t initial_append_size,
                               bool initial_sequence_mode = false)
      : TestMediaSource(filename,
                        mimetype,
                        initial_append_size,
                        initial_sequence_mode,
                        GetVivaldiTestDataFilePath(filename)) {}
};

class PlatformMediaPipelineIntegrationTest
    : public testing::Test,
      public PipelineIntegrationTestBase {
 public:
  ~PlatformMediaPipelineIntegrationTest() override {
    // Make sure the demuxer is detroyed before ipc_pipeline_test_setup_ as that
    // waits for all IPC to finish.
    if (pipeline_->IsRunning())
      Stop();

    demuxer_.reset();
  }

  void SetUp() override {
    static bool registered = false;
    if (!registered) {
      vivaldi::RegisterVivaldiPaths();
      registered = true;
    }
  }

  PipelineStatus Start(const std::string& filename, uint8_t test_type = kNormal) {
    filepath_ = GetTestDataFilePath(filename);
    std::unique_ptr<FileDataSource> file_data_source(new FileDataSource());
    CHECK(file_data_source->Initialize(filepath_))
        << "Is " << filepath_.value() << " missing?";
    return StartInternal(std::move(file_data_source), nullptr, test_type);
  }

  PipelineStatus StartVivaldi(const std::string& filename,
                              uint8_t test_type = kNormal) {
    filepath_ = GetVivaldiTestDataFilePath(filename);
    std::unique_ptr<FileDataSource> file_data_source(new FileDataSource());
    CHECK(file_data_source->Initialize(filepath_))
        << "Is " << filepath_.value() << " missing?";
    return StartInternal(std::move(file_data_source), nullptr, test_type);
  }

  Demuxer* VivaldiCreatePlatformDemuxer(
      std::unique_ptr<DataSource>& data_source,
      base::test::TaskEnvironment& task_environment_,
      MediaLog* media_log) override {
    GURL url("file://" + filepath_.AsUTF8Unsafe());
    std::string adjusted_mime_type =
        IPCDemuxer::CanPlayType(std::string(), url);
    CHECK(!adjusted_mime_type.empty());

    return new TestIPCDemuxer(data_source.get(),
                              task_environment_.GetMainThreadTaskRunner(),
                              std::move(adjusted_mime_type), media_log);
  }

  base::FilePath filepath_;
  IPCPipelineTestSetup ipc_pipeline_test_setup_;
};

TEST_F(PlatformMediaPipelineIntegrationTest, SeekWhilePaused) {
  ASSERT_EQ(PIPELINE_OK, Start("bear.mp4"));

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
  ASSERT_EQ(PIPELINE_OK, Start("bear.mp4"));

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
  ASSERT_EQ(PIPELINE_OK, Start("bear_silent.mp4", kHashed));

  Play();
  ASSERT_TRUE(Seek(pipeline_->GetMediaDuration() / 2));

  ASSERT_TRUE(WaitUntilOnEnded());
}

TEST_F(PlatformMediaPipelineIntegrationTest, TruncatedMedia) {
  ASSERT_EQ(PIPELINE_OK, StartVivaldi("vivaldi-bear_truncated.mp4"));

  Play();
  WaitUntilCurrentTimeIsAfter(base::Microseconds(1066666));
  ASSERT_TRUE(ended_ || pipeline_status_ != PIPELINE_OK);
}

TEST_F(PlatformMediaPipelineIntegrationTest, Rotated_Metadata_0) {
  ASSERT_EQ(PIPELINE_OK, Start("bear_rotate_0.mp4"));
  ASSERT_EQ(VideoRotation::VIDEO_ROTATION_0,
            metadata_.video_decoder_config.video_transformation());
}

TEST_F(PlatformMediaPipelineIntegrationTest, Rotated_Metadata_90) {
  ASSERT_EQ(PIPELINE_OK, Start("bear_rotate_90.mp4"));
  ASSERT_EQ(VideoRotation::VIDEO_ROTATION_90,
            metadata_.video_decoder_config.video_transformation());
}

TEST_F(PlatformMediaPipelineIntegrationTest, Rotated_Metadata_180) {
  ASSERT_EQ(PIPELINE_OK, Start("bear_rotate_180.mp4"));
  ASSERT_EQ(VideoRotation::VIDEO_ROTATION_180,
            metadata_.video_decoder_config.video_transformation());
}

TEST_F(PlatformMediaPipelineIntegrationTest, Rotated_Metadata_270) {
  ASSERT_EQ(PIPELINE_OK, Start("bear_rotate_270.mp4"));
  ASSERT_EQ(VideoRotation::VIDEO_ROTATION_270,
            metadata_.video_decoder_config.video_transformation());
}

TEST_F(PlatformMediaPipelineIntegrationTest,
       BasicPlayback_MediaSource_MP4_AudioOnly) {
  PlatformMediaMockMediaSource source("what-does-the-fox-say.mp4",
                                      "audio/mp4; codecs=\"mp4a.40.5\"",
                                      kAppendWholeFile);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(1493, pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Shutdown();
  Stop();
}

}  // namespace media
