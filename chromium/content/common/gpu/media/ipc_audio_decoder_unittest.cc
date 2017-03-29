// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is based on
// chromium\src\media\filters\audio_file_reader_unittest.cc.

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/common/gpu/media/test_pipeline_host.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_hash.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/filters/in_memory_url_protocol.h"
#include "media/filters/ipc_audio_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class IPCAudioDecoderTest : public testing::Test {
 public:
  IPCAudioDecoderTest() : media_thread_("MediaTest") {
    media_thread_.Start();
    media::IPCAudioDecoder::Preinitialize(
        base::Bind(&IPCAudioDecoderTest::CreateIPCMediaPipelineHost,
                   base::Unretained(this)),
        media_thread_.task_runner());
  }

  ~IPCAudioDecoderTest() override {}

  void Initialize(const std::string& filename) {
    data_ = media::ReadTestDataFile(filename);
    protocol_.reset(new media::InMemoryUrlProtocol(data_->data(),
                                                   data_->data_size(), false));
    decoder_.reset(new media::IPCAudioDecoder(protocol_.get()));
  }

  // Reads the entire file provided to Initialize().
  void ReadAndVerify(const std::string& expected_audio_hash,
                     int expected_frames) {
    scoped_ptr<media::AudioBus> decoded_audio_data = media::AudioBus::Create(
        decoder_->channels(), decoder_->number_of_frames());
    const int actual_frames = decoder_->Read(decoded_audio_data.get());
    ASSERT_LE(actual_frames, decoded_audio_data->frames());
    ASSERT_EQ(expected_frames, actual_frames);

    media::AudioHash audio_hash;
    audio_hash.Update(decoded_audio_data.get(), actual_frames);
    EXPECT_EQ(expected_audio_hash, audio_hash.ToString());
  }

  void RunTest(const std::string& filename,
               const std::string& hash,
               int channels,
               int sample_rate,
               base::TimeDelta duration,
               int frames,
               int trimmed_frames) {
    Initialize(filename);
    ASSERT_TRUE(decoder_->Initialize());
    EXPECT_EQ(channels, decoder_->channels());
    EXPECT_EQ(sample_rate, decoder_->sample_rate());
    EXPECT_EQ(duration.InMicroseconds(), decoder_->duration().InMicroseconds());
    EXPECT_EQ(frames, decoder_->number_of_frames());
    ReadAndVerify(hash, trimmed_frames);
  }

  void RunTestFailingInitialization(const std::string& filename) {
    Initialize(filename);
    EXPECT_FALSE(decoder_->Initialize());
  }

 private:
  scoped_ptr<media::IPCMediaPipelineHost> CreateIPCMediaPipelineHost(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      media::DataSource* data_source) {
    scoped_ptr<media::IPCMediaPipelineHost> ipc_media_pipeline_host;
    base::WaitableEvent ipc_media_pipeline_host_created(false, false);
    task_runner->PostTask(
        FROM_HERE,
        base::Bind(
            &IPCAudioDecoderTest::CreateIPCMediaPipelineHostOnMediaThread,
            base::Unretained(this), data_source, &ipc_media_pipeline_host,
            &ipc_media_pipeline_host_created));
    ipc_media_pipeline_host_created.Wait();
    return ipc_media_pipeline_host.Pass();
  }

  void CreateIPCMediaPipelineHostOnMediaThread(
      media::DataSource* data_source,
      scoped_ptr<media::IPCMediaPipelineHost>* host,
      base::WaitableEvent* created) {
    CHECK(media_thread_.task_runner()->BelongsToCurrentThread());
    host->reset(new TestPipelineHost(data_source));
    created->Signal();
  }

  base::Thread media_thread_;
  scoped_refptr<media::DecoderBuffer> data_;
  scoped_ptr<media::InMemoryUrlProtocol> protocol_;
  scoped_ptr<media::IPCAudioDecoder> decoder_;

  DISALLOW_COPY_AND_ASSIGN(IPCAudioDecoderTest);
};

#if defined(OS_MACOSX) || defined(OS_WIN)
// TODO(pgraszka): We need to investigate why there are so big differences
// between expected test values on both platfroms. The investigation is
// conducted in DNA-41263.

TEST_F(IPCAudioDecoderTest, MP3) {
  RunTest("sfx.mp3",
#if defined(OS_MACOSX)
          "0.83,1.07,2.28,3.57,3.98,3.20,",
#elif defined(OS_WIN)
          "0.35,1.24,2.97,4.28,4.18,2.75,",
#endif
          1, 44100,
#if defined(OS_MACOSX)
          base::TimeDelta::FromMicroseconds(287346), 12672, 12672);
#elif defined(OS_WIN)
          base::TimeDelta::FromMicroseconds(313469), 13824, 13824);
#endif
}

TEST_F(IPCAudioDecoderTest, CorruptMP3) {
  RunTest("corrupt.mp3",
#if defined(OS_MACOSX)
          "-2.44,-0.74,1.48,2.49,1.45,-1.47,",
#elif defined(OS_WIN)
          "-5.04,-3.03,-0.53,1.08,0.23,-2.29,",
#endif
          1, 44100,
#if defined(OS_MACOSX)
          base::TimeDelta::FromMicroseconds(1018775), 44928,
#elif defined(OS_WIN)
          base::TimeDelta::FromMicroseconds(1018800), 44930,
#endif
          44928);
}

TEST_F(IPCAudioDecoderTest, AAC) {
  RunTest("sfx.m4a",
#if defined(OS_MACOSX)
          "-5.29,-5.47,-5.05,-4.33,-2.99,-3.79,",
#elif defined(OS_WIN)
          "2.62,3.23,2.38,2.56,2.75,2.73,",
#endif
          1, 44100, base::TimeDelta::FromMicroseconds(312000), 13760,
#if defined(OS_MACOSX)
          11200);
#elif defined(OS_WIN)
          13760);
#endif
}

TEST_F(IPCAudioDecoderTest, InvalidFile) {
  RunTestFailingInitialization("ten_byte_file");
}

#endif  // defined(OS_MACOSX) || defined(OS_WIN)

}  // namespace content
