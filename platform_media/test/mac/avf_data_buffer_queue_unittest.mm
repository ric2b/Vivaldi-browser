// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/gpu/decoders/mac/avf_data_buffer_queue.h"

#include "base/bind.h"
#include "media/base/data_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

const base::TimeDelta kCapacity = base::TimeDelta::FromMicroseconds(16);

scoped_refptr<DataBuffer> CreateBuffer(const base::TimeDelta& timestamp,
                                              const base::TimeDelta& duration) {
  scoped_refptr<DataBuffer> buffer(new DataBuffer(1));
  buffer->set_timestamp(timestamp);
  buffer->set_duration(duration);
  return buffer;
}

class AVFDataBufferQueueTest : public testing::Test {
 public:
  AVFDataBufferQueueTest()
      : queue_(AVFDataBufferQueue::Type(),
               kCapacity,
               base::Bind(&AVFDataBufferQueueTest::CapacityAvailable,
                          base::Unretained(this)),
               base::Bind(&AVFDataBufferQueueTest::CapacityDepleted,
                          base::Unretained(this))),
        capacity_available_(false),
        capacity_depleted_(false) {}

 protected:
  AVFDataBufferQueue::ReadCB read_cb() {
    return base::Bind(&AVFDataBufferQueueTest::OnRead, base::Unretained(this));
  }

  AVFDataBufferQueue queue_;
  bool capacity_available_;
  bool capacity_depleted_;
  scoped_refptr<DataBuffer> last_buffer_read_;

 private:
  void CapacityAvailable() { capacity_available_ = true; }
  void CapacityDepleted() { capacity_depleted_ = true; }
  void OnRead(const scoped_refptr<DataBuffer>& buffer) {
    last_buffer_read_ = buffer;
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

  last_buffer_read_ = NULL;
  queue_.Read(read_cb());
  ASSERT_FALSE(last_buffer_read_.get() == NULL);
  EXPECT_FALSE(capacity_available_);

  last_buffer_read_ = NULL;
  queue_.Read(read_cb());
  EXPECT_FALSE(last_buffer_read_.get() == NULL);
  EXPECT_TRUE(capacity_available_);
}

TEST_F(AVFDataBufferQueueTest, CatchUpMode) {
  // Normally, we want the queue to collect some buffers before it starts to
  // return them.

  queue_.BufferReady(CreateBuffer(base::TimeDelta(), kCapacity / 2));

  last_buffer_read_ = NULL;
  queue_.Read(read_cb());
  ASSERT_TRUE(last_buffer_read_.get() == NULL);

  queue_.BufferReady(CreateBuffer(kCapacity / 2, kCapacity / 2));

  ASSERT_TRUE(last_buffer_read_.get() == NULL);

  // But when we enter the "catching-up" mode, we want all the buffers it's
  // got, NOW!

  queue_.BufferReady(CreateBuffer(kCapacity, kCapacity / 2));

  ASSERT_FALSE(last_buffer_read_.get() == NULL);

  last_buffer_read_ = NULL;
  queue_.Read(read_cb());
  ASSERT_FALSE(last_buffer_read_.get() == NULL);

  last_buffer_read_ = NULL;
  queue_.Read(read_cb());
  ASSERT_FALSE(last_buffer_read_.get() == NULL);

  // An empty queue should take the time to collect some buffers again before
  // it resumes returning them.

  queue_.BufferReady(CreateBuffer(3 * kCapacity / 2, kCapacity / 2));

  last_buffer_read_ = NULL;
  queue_.Read(read_cb());
  ASSERT_TRUE(last_buffer_read_.get() == NULL);
}

TEST_F(AVFDataBufferQueueTest, Flush) {
  queue_.BufferReady(CreateBuffer(base::TimeDelta(), kCapacity));
  queue_.Flush();

  last_buffer_read_ = NULL;
  queue_.Read(read_cb());
  EXPECT_TRUE(last_buffer_read_.get() == NULL);
}

TEST_F(AVFDataBufferQueueTest, EndOfStream) {
  queue_.BufferReady(CreateBuffer(base::TimeDelta(), kCapacity));

  last_buffer_read_ = NULL;
  queue_.Read(read_cb());
  ASSERT_TRUE(last_buffer_read_.get() == NULL);

  queue_.SetEndOfStream();

  ASSERT_FALSE(last_buffer_read_.get() == NULL);
  EXPECT_FALSE(last_buffer_read_->end_of_stream());

  last_buffer_read_ = NULL;
  queue_.Read(read_cb());
  ASSERT_FALSE(last_buffer_read_.get() == NULL);
  EXPECT_TRUE(last_buffer_read_->end_of_stream());

  // All further reads should return the EOS buffer, too.
  last_buffer_read_ = NULL;
  queue_.Read(read_cb());
  ASSERT_FALSE(last_buffer_read_.get() == NULL);
  EXPECT_TRUE(last_buffer_read_->end_of_stream());
}

}  // namespace
}  // namespace media
