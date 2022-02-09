// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/gpu/decoders/mac/avf_data_buffer_queue.h"

#include "base/bind.h"
#include "common/platform_media_pipeline_types.h"
#include "media/base/data_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

const base::TimeDelta kCapacity = base::TimeDelta::FromMicroseconds(16);

scoped_refptr<DataBuffer> CreateBuffer(const base::TimeDelta& timestamp,
                                       const base::TimeDelta& duration) {
  scoped_refptr<DataBuffer> buffer(new DataBuffer(1));
  buffer->set_data_size(1);
  buffer->writable_data()[0] = 42;
  buffer->set_timestamp(timestamp);
  buffer->set_duration(duration);
  return buffer;
}

class AVFDataBufferQueueTest : public testing::Test {
 public:
  AVFDataBufferQueueTest()
      : queue_(PlatformStreamType::kVideo,
               kCapacity,
               base::BindRepeating(&AVFDataBufferQueueTest::CapacityAvailable,
                                   base::Unretained(this)),
               base::BindRepeating(&AVFDataBufferQueueTest::CapacityDepleted,
                                   base::Unretained(this))),
        capacity_available_(false),
        capacity_depleted_(false) {
    ipc_buffer_.Init(PlatformStreamType::kAudio);
    ipc_buffer_.set_reply_cb(base::BindRepeating(
        &AVFDataBufferQueueTest::OnRead, base::Unretained(this)));
  }

 protected:
  IPCDecodingBuffer start_read() {
    CHECK(ipc_buffer_);
    ipc_buffer_.set_status(MediaDataStatus::kMediaError);
    last_read_status_.reset();
    return std::move(ipc_buffer_);
  }

  AVFDataBufferQueue queue_;
  IPCDecodingBuffer ipc_buffer_;
  bool capacity_available_;
  bool capacity_depleted_;
  absl::optional<MediaDataStatus> last_read_status_;

 private:
  void CapacityAvailable() { capacity_available_ = true; }
  void CapacityDepleted() { capacity_depleted_ = true; }
  void OnRead(IPCDecodingBuffer ipc_buffer) {
    ASSERT_TRUE(ipc_buffer);
    ASSERT_FALSE(ipc_buffer_);
    last_read_status_ = ipc_buffer.status();
    if (*last_read_status_ == MediaDataStatus::kOk) {
      ASSERT_EQ(ipc_buffer.data_size(), 1);
      ASSERT_EQ(ipc_buffer.GetDataForTests()[0], 42);
    }
    ipc_buffer_ = std::move(ipc_buffer);
  }
};

TEST_F(AVFDataBufferQueueTest, DepleteCapacity) {
  ASSERT_TRUE(queue_.HasAvailableCapacity());
  ASSERT_FALSE(capacity_depleted_);

  queue_.BufferReady(CreateBuffer(base::TimeDelta(), kCapacity));
  EXPECT_TRUE(queue_.HasAvailableCapacity());
  EXPECT_FALSE(capacity_depleted_);

  queue_.BufferReady(CreateBuffer(kCapacity, kCapacity));
  EXPECT_FALSE(queue_.HasAvailableCapacity());
  EXPECT_TRUE(capacity_depleted_);
}

TEST_F(AVFDataBufferQueueTest, FreeCapacity) {
  queue_.BufferReady(CreateBuffer(base::TimeDelta(), kCapacity));
  queue_.BufferReady(CreateBuffer(kCapacity, kCapacity));
  queue_.BufferReady(CreateBuffer(kCapacity * 2, kCapacity));
  ASSERT_FALSE(queue_.HasAvailableCapacity());
  capacity_available_ = false;

  queue_.Read(start_read());
  ASSERT_EQ(last_read_status_, MediaDataStatus::kOk);
  EXPECT_FALSE(capacity_available_);

  queue_.Read(start_read());
  ASSERT_EQ(last_read_status_, MediaDataStatus::kOk);
  EXPECT_TRUE(capacity_available_);
}

TEST_F(AVFDataBufferQueueTest, CatchUpMode) {
  // Normally, we want the queue to collect some buffers before it starts to
  // return them.

  queue_.BufferReady(CreateBuffer(base::TimeDelta(), kCapacity / 2));

  queue_.Read(start_read());
  ASSERT_FALSE(last_read_status_);

  queue_.BufferReady(CreateBuffer(kCapacity / 2, kCapacity / 2));

  ASSERT_FALSE(last_read_status_);

  // But when we enter the "catching-up" mode, we want all the buffers it's
  // got, NOW!

  queue_.BufferReady(CreateBuffer(kCapacity, kCapacity / 2));

  ASSERT_EQ(last_read_status_, MediaDataStatus::kOk);

  queue_.Read(start_read());
  ASSERT_EQ(last_read_status_, MediaDataStatus::kOk);

  queue_.Read(start_read());
  ASSERT_EQ(last_read_status_, MediaDataStatus::kOk);

  // An empty queue should take the time to collect some buffers again before
  // it resumes returning them.

  queue_.BufferReady(CreateBuffer(3 * kCapacity / 2, kCapacity / 2));

  queue_.Read(start_read());
  ASSERT_FALSE(last_read_status_);
}

TEST_F(AVFDataBufferQueueTest, Flush) {
  queue_.BufferReady(CreateBuffer(base::TimeDelta(), kCapacity));
  queue_.Flush();

  queue_.Read(start_read());
  ASSERT_FALSE(last_read_status_);
}

TEST_F(AVFDataBufferQueueTest, EndOfStream) {
  queue_.BufferReady(CreateBuffer(base::TimeDelta(), kCapacity));

  queue_.Read(start_read());
  ASSERT_FALSE(last_read_status_);

  queue_.SetEndOfStream();

  ASSERT_EQ(last_read_status_, MediaDataStatus::kOk);

  queue_.Read(start_read());
  ASSERT_EQ(last_read_status_, MediaDataStatus::kEOS);

  // All further reads should return the EOS buffer, too.
  queue_.Read(start_read());
  ASSERT_EQ(last_read_status_, MediaDataStatus::kEOS);
}

}  // namespace
}  // namespace media
