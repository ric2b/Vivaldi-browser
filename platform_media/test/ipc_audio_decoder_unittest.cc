// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is based on
// chromium\src\media\filters\audio_file_reader_unittest.cc.

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread.h"
#include "platform_media/test/test_pipeline_host.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_hash.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/filters/in_memory_url_protocol.h"
#include "platform_media/renderer/decoders/ipc_audio_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

namespace media {

namespace {
std::unique_ptr<IPCMediaPipelineHost> CreateIPCMediaPipelineHost(
    const scoped_refptr<base::SequencedTaskRunner>& decode_task_runner,
    DataSource* data_source) {
  return base::WrapUnique(new TestPipelineHost(data_source));
}
}  // namespace

class IPCAudioDecoderTest : public testing::Test {
 public:
  IPCAudioDecoderTest() : decode_thread_(__FUNCTION__) {
    if (!IPCFactory::IsAvailable())
      return;

    CHECK(decode_thread_.Start());

    IPCFactory::Preinitialize(
        base::Bind(&CreateIPCMediaPipelineHost), decode_thread_.task_runner(),
        decode_thread_.task_runner());
  }

  bool Initialize(const std::string& filename) {
    if (!IPCFactory::IsAvailable()) {
      LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__
                << " IPCAudioDecoder not available on this platform"
                << " skipping test";
      return false;
    }

    data_ = ReadTestDataFile(filename);
    protocol_.reset(new InMemoryUrlProtocol(data_->data(),
                                                   data_->data_size(), false));
    decoder_.reset(new IPCAudioDecoder(protocol_.get()));
    return true;
  }

  // Reads the entire file provided to Initialize().
  void ReadAndVerify(const std::string& expected_audio_hash,
                     int expected_frames) {
    std::vector<std::unique_ptr<AudioBus>> decoded_audio_packets;
    int actual_frames = decoder_->Read(&decoded_audio_packets);
    std::unique_ptr<AudioBus> decoded_audio_data =
        AudioBus::Create(decoder_->channels(), actual_frames);
    int dest_start_frame = 0;
    for (size_t k = 0; k < decoded_audio_packets.size(); ++k) {
      const AudioBus* packet = decoded_audio_packets[k].get();
      int frame_count = packet->frames();
      packet->CopyPartialFramesTo(0, frame_count, dest_start_frame,
                                  decoded_audio_data.get());
      dest_start_frame += frame_count;
    }
    ASSERT_LE(actual_frames, decoded_audio_data->frames());
    ASSERT_EQ(expected_frames, actual_frames);

    AudioHash audio_hash;
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
  scoped_refptr<DecoderBuffer> data_;
  std::unique_ptr<InMemoryUrlProtocol> protocol_;
  std::unique_ptr<IPCAudioDecoder> decoder_;

  DISALLOW_COPY_AND_ASSIGN(IPCAudioDecoderTest);
};

// Note: The expected results are partly decoder-dependent.  The same
// differences in duration, etc., occur when decoding via IPCDemuxer.

TEST_F(IPCAudioDecoderTest, AAC) {
  RunTest("sfx.m4a",
#if defined(OS_MACOSX)
          "-5.29,-5.47,-5.05,-4.33,-2.99,-3.79,", 1, 44100,
          base::TimeDelta::FromMicroseconds(312000), 13760, 11200);
#elif defined(OS_WIN) || defined(OS_LINUX)
          "2.62,3.23,2.38,2.56,2.75,2.73,", 1, 44100,
          base::TimeDelta::FromMicroseconds(312000), 13760, 13760);
#endif
}

TEST_F(IPCAudioDecoderTest, InvalidFile) {
  RunTestFailingInitialization("ten_byte_file");
}

}  // namespace media
