// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is based on
// chromium\src\media\filters\audio_file_reader_unittest.cc.

#include "base/bind.h"
#include "base/memory/ptr_util.h"
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

namespace {
std::unique_ptr<media::IPCMediaPipelineHost> CreateIPCMediaPipelineHost(
    const scoped_refptr<base::SequencedTaskRunner>& decode_task_runner,
    media::DataSource* data_source) {
  return base::WrapUnique(new TestPipelineHost(data_source));
}
}  // namespace

class IPCAudioDecoderTest : public testing::Test {
 public:
  IPCAudioDecoderTest() : decode_thread_(__FUNCTION__) {
    if (!media::IPCAudioDecoder::IsAvailable())
      return;

    CHECK(decode_thread_.Start());

    media::IPCAudioDecoder::Preinitialize(
        base::Bind(&CreateIPCMediaPipelineHost), decode_thread_.task_runner(),
        decode_thread_.task_runner());
  }

  bool Initialize(const std::string& filename) {
    if (!media::IPCAudioDecoder::IsAvailable()) {
      LOG(INFO)
          << "IPCAudioDecoder not available on this platform, skipping test";
      return false;
    }

    data_ = media::ReadTestDataFile(filename);
    protocol_.reset(new media::InMemoryUrlProtocol(data_->data(),
                                                   data_->data_size(), false));
    decoder_.reset(new media::IPCAudioDecoder(protocol_.get()));
    return true;
  }

  // Reads the entire file provided to Initialize().
  void ReadAndVerify(const std::string& expected_audio_hash,
                     int expected_frames) {
    std::unique_ptr<media::AudioBus> decoded_audio_data = media::AudioBus::Create(
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
    if (!Initialize(filename))
      return;

    ASSERT_TRUE(decoder_->Initialize());

    EXPECT_EQ(channels, decoder_->channels());
    EXPECT_EQ(sample_rate, decoder_->sample_rate());
    EXPECT_EQ(duration.InMicroseconds(), decoder_->duration().InMicroseconds());
    EXPECT_EQ(frames, decoder_->number_of_frames());
    ReadAndVerify(hash, trimmed_frames);
  }

  void RunTestFailingInitialization(const std::string& filename) {
    if (!Initialize(filename))
      return;

    Initialize(filename);
    EXPECT_FALSE(decoder_->Initialize());
  }

 private:
  base::Thread decode_thread_;
  scoped_refptr<media::DecoderBuffer> data_;
  std::unique_ptr<media::InMemoryUrlProtocol> protocol_;
  std::unique_ptr<media::IPCAudioDecoder> decoder_;

  DISALLOW_COPY_AND_ASSIGN(IPCAudioDecoderTest);
};

// Note: The expected results are partly decoder-dependent.  The same
// differences in duration, etc., occur when decoding via IPCDemuxer.

TEST_F(IPCAudioDecoderTest, MP3) {
  RunTest("sfx.mp3",
#if defined(OS_MACOSX)
          "0.83,1.07,2.28,3.57,3.98,3.20,", 1, 44100,
          base::TimeDelta::FromMicroseconds(287346), 12672, 12672);
#elif defined(OS_WIN)
          "0.35,1.24,2.97,4.28,4.18,2.75,", 1, 44100,
          base::TimeDelta::FromMicroseconds(313469), 13824, 13824);
#endif
}

TEST_F(IPCAudioDecoderTest, CorruptMP3) {
  RunTest("corrupt.mp3",
#if defined(OS_MACOSX)
          "-2.44,-0.74,1.48,2.49,1.45,-1.47,", 1, 44100,
          base::TimeDelta::FromMicroseconds(1018775), 44928, 44928);
#elif defined(OS_WIN)
          "-5.04,-3.03,-0.53,1.08,0.23,-2.29,", 1, 44100,
          base::TimeDelta::FromMicroseconds(1018800), 44930, 44928);
#endif
}

TEST_F(IPCAudioDecoderTest, AAC) {
  RunTest("sfx.m4a",
#if defined(OS_MACOSX)
          "-5.29,-5.47,-5.05,-4.33,-2.99,-3.79,", 1, 44100,
          base::TimeDelta::FromMicroseconds(312000), 13760, 11200);
#elif defined(OS_WIN)
          "2.62,3.23,2.38,2.56,2.75,2.73,", 1, 44100,
          base::TimeDelta::FromMicroseconds(312000), 13760, 13760);
#endif
}

TEST_F(IPCAudioDecoderTest, InvalidFile) {
  RunTestFailingInitialization("ten_byte_file");
}

}  // namespace content
