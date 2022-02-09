// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is based on
// chromium\src\media\filters\audio_file_reader_unittest.cc.

#include "platform_media/renderer/decoders/ipc_audio_decoder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_hash.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/filters/in_memory_url_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "platform_media/test/ipc_pipeline_test_setup.h"

namespace media {


class IPCAudioDecoderTest : public testing::Test {
 public:
  IPCAudioDecoderTest() = default;

  struct DecoderData {
    scoped_refptr<DecoderBuffer> buffer;
    std::unique_ptr<InMemoryUrlProtocol> protocol;
    std::unique_ptr<IPCAudioDecoder> decoder;
  };

  std::unique_ptr<DecoderData> Initialize(const std::string& filename) {
    auto data = std::make_unique<DecoderData>();
    data->buffer = ReadTestDataFile(filename);
    data->protocol = std::make_unique<InMemoryUrlProtocol>(
        data->buffer->data(), data->buffer->data_size(), false);
    data->decoder = std::make_unique<IPCAudioDecoder>(data->protocol.get());
    return data;
  }

  // Reads the entire file provided to Initialize().
  void ReadAndVerify(const std::string& expected_audio_hash,
                     int trimmed_frames_min,
                     IPCAudioDecoder& decoder) {
    std::vector<std::unique_ptr<AudioBus>> decoded_audio_packets;
    int actual_frames = decoder.Read(&decoded_audio_packets);
    std::unique_ptr<AudioBus> decoded_audio_data =
        AudioBus::Create(decoder.channels(), actual_frames);
    int dest_start_frame = 0;
    for (size_t k = 0; k < decoded_audio_packets.size(); ++k) {
      const AudioBus* packet = decoded_audio_packets[k].get();
      int frame_count = packet->frames();
      packet->CopyPartialFramesTo(0, frame_count, dest_start_frame,
                                  decoded_audio_data.get());
      dest_start_frame += frame_count;
    }
    EXPECT_LE(actual_frames, decoded_audio_data->frames());
    EXPECT_LE(trimmed_frames_min, actual_frames);

    AudioHash audio_hash;
    audio_hash.Update(decoded_audio_data.get(), actual_frames);

    // TODO(igor@vivaldi.com): Figure out how to verify this on Mac where the
    // number of actual frames depends on OS version etc.
#if !defined(OS_MAC)
    EXPECT_EQ(expected_audio_hash, audio_hash.ToString());
#endif
  }

  void RunTest(const std::string& filename,
               const std::string& hash,
               int channels,
               int sample_rate,
               base::TimeDelta duration,
               int frames,
               int trimmed_frames_min) {
    std::unique_ptr<DecoderData> data = Initialize(filename);
    bool ok = data->decoder->Initialize();
    bool available = IPCAudioDecoder::IsAvailable();

    // If decoder libraries is available, Initialize() must succeed.
    // If Initialize() succeeded, the libraries must be available.
    EXPECT_EQ(ok, available);
    if (!ok && !available) {
      LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__
                << " IPCAudioDecoder not available on this platform"
                << " skipping test";
      GTEST_SKIP();
      return;
    }

    EXPECT_EQ(channels, data->decoder->channels());
    EXPECT_EQ(sample_rate, data->decoder->sample_rate());
    EXPECT_EQ(duration.InMicroseconds(),
              data->decoder->duration().InMicroseconds());
    EXPECT_EQ(frames, data->decoder->number_of_frames());
    ReadAndVerify(hash, trimmed_frames_min, *data->decoder);
  }

 protected:
  base::test::TaskEnvironment task_environment_{};
  IPCPipelineTestSetup test_setup_;

  DISALLOW_COPY_AND_ASSIGN(IPCAudioDecoderTest);
};

// Note: The expected results are partly decoder-dependent.  The same
// differences in duration, etc., occur when decoding via IPCDemuxer.

TEST_F(IPCAudioDecoderTest, AAC) {
  RunTest("sfx.m4a",
#if defined(OS_MAC)
          // On Mac hash is not compared.
          "-4.72,-4.77,-4.73,-4.63,-4.53,-3.78,", 1, 44100,
          base::TimeDelta::FromMicroseconds(312000), 13760, 10607
#elif defined(OS_WIN) || defined(OS_LINUX)
          "2.62,3.23,2.38,2.56,2.75,2.73,", 1, 44100,
          base::TimeDelta::FromMicroseconds(312000), 13760, 13760
#endif
  );
}

TEST_F(IPCAudioDecoderTest, InvalidFile) {
  std::unique_ptr<DecoderData> data = Initialize("ten_byte_file");
  EXPECT_FALSE(data->decoder->Initialize());
}

}  // namespace media
