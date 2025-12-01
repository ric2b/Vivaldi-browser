// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/socket_handle_waiter_posix.h"

#include <sys/socket.h>
#include <unistd.h>  // For pipe() and close()

#include <cerrno>  // For errno
#include <chrono>
#include <iostream>
#include <thread>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/impl/socket_handle_posix.h"
#include "platform/impl/socket_handle_waiter.h"
#include "platform/impl/timeval_posix.h"
#include "platform/test/fake_clock.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::Return;

namespace openscreen {
namespace {

class MockSubscriber : public SocketHandleWaiter::Subscriber {
 public:
  using SocketHandleRef = SocketHandleWaiter::SocketHandleRef;
  MOCK_METHOD(void, ProcessReadyHandle, (SocketHandleRef, uint32_t));
  MOCK_METHOD(bool, HasPendingWrite, (SocketHandleRef));
};

class TestingSocketHandleWaiter : public SocketHandleWaiter {
 public:
  // These protected fields need to be public for testing below.
  using SocketHandleRef = SocketHandleWaiter::SocketHandleRef;
  using HandleWithFlags = SocketHandleWaiter::HandleWithFlags;

  TestingSocketHandleWaiter() : SocketHandleWaiter(&FakeClock::now) {}

  MOCK_METHOD(ErrorOr<std::vector<HandleWithFlags>>,
              AwaitSocketsReady,
              (const std::vector<HandleWithFlags>&, const Clock::duration&),
              (override));

  FakeClock fake_clock{Clock::time_point{Clock::duration{1234567}}};
};

}  // namespace

// Test fixture for tests that need an instance of SocketHandleWaiterPosix with
// an actual pipe.
class SocketHandleWaiterPosixInstanceTest : public ::testing::Test {
 protected:
  SocketHandleWaiterPosixInstanceTest()
      : clock_(Clock::time_point{Clock::duration{1234567}}),
        waiter_(&clock_.now) {}

  void TearDown() override {
    // Clean up any FDs created in tests if they weren't closed properly.
    // This is more of a safeguard.
    for (int fd : fds_to_close_) {
      close(fd);
    }
    fds_to_close_.clear();
  }

  // Helper to create a pipe and register fds for cleanup.
  void CreatePipe(int pipe_fds[2]) {
    ASSERT_NE(-1, pipe(pipe_fds))
        << "Failed to create pipe: " << strerror(errno);
    fds_to_close_.push_back(pipe_fds[0]);
    fds_to_close_.push_back(pipe_fds[1]);
  }

  void ClosePipe(int pipe_fds[2]) {
    auto remove_fd = [this](int fd_to_remove) {
      fds_to_close_.erase(
          std::remove(fds_to_close_.begin(), fds_to_close_.end(), fd_to_remove),
          fds_to_close_.end());
    };

    close(pipe_fds[0]);
    remove_fd(pipe_fds[0]);
    close(pipe_fds[1]);
    remove_fd(pipe_fds[1]);
  }

  FakeClock clock_;
  SocketHandleWaiterPosix waiter_;  // The actual class under test
  MockSubscriber subscriber_;
  std::vector<int> fds_to_close_;
};

TEST(SocketHandleWaiterBaseTest, BubblesUpAwaitSocketsReadyErrors) {
  MockSubscriber subscriber;
  TestingSocketHandleWaiter waiter;
  SocketHandle handle0(0);
  SocketHandle handle1(1);
  SocketHandle handle2(2);
  const SocketHandle& handle0_ref = handle0;
  const SocketHandle& handle1_ref = handle1;
  const SocketHandle& handle2_ref = handle2;

  waiter.Subscribe(&subscriber, std::cref(handle0_ref),
                   SocketHandleWaiter::kReadWriteFlags);
  waiter.Subscribe(&subscriber, std::cref(handle1_ref),
                   SocketHandleWaiter::kReadWriteFlags);
  waiter.Subscribe(&subscriber, std::cref(handle2_ref),
                   SocketHandleWaiter::kReadWriteFlags);
  Error::Code response = Error::Code::kAgain;
  EXPECT_CALL(subscriber, ProcessReadyHandle(_, _)).Times(0);
  EXPECT_CALL(waiter, AwaitSocketsReady(_, _))
      .WillOnce(Return(ByMove(response)));
  waiter.ProcessHandles(Clock::duration{0});
}

TEST(SocketHandleWaiterBaseTest, WatchedSocketsReturnedToCorrectSubscribers) {
  MockSubscriber subscriber;
  MockSubscriber subscriber2;
  TestingSocketHandleWaiter waiter;
  SocketHandle handle0(0);
  SocketHandle handle1(1);
  SocketHandle handle2(2);
  SocketHandle handle3(3);
  const SocketHandle& handle0_ref = handle0;
  const SocketHandle& handle1_ref = handle1;
  const SocketHandle& handle2_ref = handle2;
  const SocketHandle& handle3_ref = handle3;

  waiter.Subscribe(&subscriber, std::cref(handle0_ref),
                   SocketHandleWaiter::kReadWriteFlags);
  waiter.Subscribe(&subscriber, std::cref(handle2_ref),
                   SocketHandleWaiter::kReadWriteFlags);
  waiter.Subscribe(&subscriber2, std::cref(handle1_ref),
                   SocketHandleWaiter::kReadWriteFlags);
  waiter.Subscribe(&subscriber2, std::cref(handle3_ref),
                   SocketHandleWaiter::kReadWriteFlags);

  EXPECT_CALL(subscriber, ProcessReadyHandle(std::cref(handle0_ref),
                                             SocketHandleWaiter::kReadable))
      .Times(1);
  EXPECT_CALL(subscriber, ProcessReadyHandle(std::cref(handle2_ref),
                                             SocketHandleWaiter::kWritable))
      .Times(1);
  EXPECT_CALL(subscriber2, ProcessReadyHandle(std::cref(handle1_ref),
                                              SocketHandleWaiter::kReadable))
      .Times(1);
  EXPECT_CALL(subscriber2,
              ProcessReadyHandle(std::cref(handle3_ref),
                                 SocketHandleWaiter::kReadWriteFlags))
      .Times(1);
  EXPECT_CALL(waiter, AwaitSocketsReady(_, _))
      .WillOnce(
          Return(ByMove(std::vector<TestingSocketHandleWaiter::HandleWithFlags>{
              {std::cref(handle0_ref), SocketHandleWaiter::kReadable},
              {std::cref(handle1_ref), SocketHandleWaiter::kReadable},
              {std::cref(handle2_ref), SocketHandleWaiter::kWritable},
              {std::cref(handle3_ref), SocketHandleWaiter::kReadWriteFlags}})));
  waiter.ProcessHandles(Clock::duration{0});
}

TEST(SocketHandleWaiterBaseTest, HandlesNoSubscriptions) {
  TestingSocketHandleWaiter waiter;

  EXPECT_CALL(waiter, AwaitSocketsReady(_, _)).Times(0);
  Error result = waiter.ProcessHandles(Clock::duration{0});
  EXPECT_EQ(result.code(), Error::Code::kAgain);
}

TEST(SocketHandleWaiterBaseTest, UnsubscribeRemovesHandle) {
  MockSubscriber subscriber;
  TestingSocketHandleWaiter waiter;
  SocketHandle handle(123);

  waiter.Subscribe(&subscriber, handle, SocketHandleWaiter::Flags::kReadable);
  waiter.Unsubscribe(&subscriber, handle);

  EXPECT_CALL(waiter, AwaitSocketsReady(_, _)).Times(0);
  Error result = waiter.ProcessHandles(Clock::duration{0});
  EXPECT_EQ(result.code(), Error::Code::kAgain);
}

TEST(SocketHandleWaiterBaseTest,
     UnsubscribeAllRemovesOnlySubscriptionsForProvidedSubscriber) {
  MockSubscriber subscriber1;
  MockSubscriber subscriber2;
  TestingSocketHandleWaiter waiter;
  SocketHandle handle0(0);
  SocketHandle handle1(1);
  SocketHandle handle2(2);

  waiter.Subscribe(&subscriber1, handle0, SocketHandleWaiter::kReadable);
  waiter.Subscribe(&subscriber1, handle1, SocketHandleWaiter::kReadable);
  waiter.Subscribe(&subscriber2, handle2, SocketHandleWaiter::kReadable);

  waiter.UnsubscribeAll(&subscriber1);

  EXPECT_CALL(subscriber1, ProcessReadyHandle(_, _)).Times(0);
  EXPECT_CALL(subscriber2, ProcessReadyHandle(std::cref(handle2),
                                              SocketHandleWaiter::kReadable))
      .Times(1);

  EXPECT_CALL(
      waiter,
      AwaitSocketsReady(
          testing::Truly(
              [&](const std::vector<TestingSocketHandleWaiter::HandleWithFlags>&
                      handle_list) {
                return handle_list.size() == 1 &&
                       handle_list[0].handle.get() == handle2 &&
                       handle_list[0].flags == SocketHandleWaiter::kReadable;
              }),
          _))
      .WillOnce(
          Return(ByMove(std::vector<TestingSocketHandleWaiter::HandleWithFlags>{
              {handle2, SocketHandleWaiter::kReadable}})));

  waiter.ProcessHandles(Clock::duration{0});
}

TEST(SocketHandleWaiterBaseTest,
     AwaitSocketsReadyCalledWithCorrectSubscribedFlags) {
  MockSubscriber subscriber;
  TestingSocketHandleWaiter waiter;
  SocketHandle handle0(0);
  constexpr uint32_t subscribed_flags = SocketHandleWaiter::Flags::kReadable;
  // AwaitSocketsReady might report more flags if the underlying mechanism
  // does.
  constexpr uint32_t reported_flags = SocketHandleWaiter::Flags::kReadable |
                                      SocketHandleWaiter::Flags::kWritable;

  waiter.Subscribe(&subscriber, handle0, subscribed_flags);

  EXPECT_CALL(
      waiter,
      AwaitSocketsReady(
          testing::Truly(
              [&](const std::vector<TestingSocketHandleWaiter::HandleWithFlags>&
                      handle_list) {
                return handle_list.size() == 1 &&
                       handle_list[0].handle.get() == handle0 &&
                       handle_list[0].flags == subscribed_flags;
              }),
          _))
      .WillOnce(
          Return(ByMove(std::vector<TestingSocketHandleWaiter::HandleWithFlags>{
              {handle0, reported_flags}})));

  // The subscriber receives the flags reported by AwaitSocketsReady.
  EXPECT_CALL(subscriber,
              ProcessReadyHandle(std::cref(handle0), reported_flags))
      .Times(1);

  waiter.ProcessHandles(Clock::duration{0});
}

TEST(SocketHandleWaiterBaseTest, SubsequentSubscribeForSameHandleIsIgnored) {
  MockSubscriber subscriber1;
  MockSubscriber subscriber2;
  TestingSocketHandleWaiter waiter;
  SocketHandle handle0(0);

  waiter.Subscribe(&subscriber1, handle0, SocketHandleWaiter::Flags::kReadable);
  // This second subscribe for the same handle should be ignored.
  waiter.Subscribe(&subscriber2, handle0, SocketHandleWaiter::Flags::kWritable);

  EXPECT_CALL(
      waiter,
      AwaitSocketsReady(
          testing::Truly(
              [&](const std::vector<TestingSocketHandleWaiter::HandleWithFlags>&
                      handle_list) {
                return handle_list.size() == 1 &&
                       handle_list[0].handle.get() == handle0 &&
                       handle_list[0].flags ==
                           SocketHandleWaiter::Flags::kReadable;
              }),
          _))
      .WillOnce(Return(Error::Code::kAgain));

  EXPECT_CALL(subscriber1, ProcessReadyHandle(_, _)).Times(0);
  EXPECT_CALL(subscriber2, ProcessReadyHandle(_, _)).Times(0);

  waiter.ProcessHandles(Clock::duration{0});
}

TEST(SocketHandleWaiterBaseTest, OnHandleDeletionRemovesHandle) {
  MockSubscriber subscriber;
  TestingSocketHandleWaiter waiter;
  SocketHandle handle0(0);

  waiter.Subscribe(&subscriber, handle0, SocketHandleWaiter::Flags::kReadable);
  waiter.OnHandleDeletion(&subscriber, handle0, true);

  EXPECT_CALL(waiter, AwaitSocketsReady(IsEmpty(), _)).Times(0);
  const Error result = waiter.ProcessHandles(Clock::duration{0});
  EXPECT_EQ(result.code(), Error::Code::kAgain);
}

TEST(SocketHandleWaiterBaseTest,
     ProcessHandlesIgnoresUnmappedHandlesFromAwaitSocketsReady) {
  MockSubscriber subscriber;
  TestingSocketHandleWaiter waiter;
  SocketHandle handle0(0);  // Subscribed
  SocketHandle handle1(1);  // Not subscribed, but returned by AwaitSocketsReady

  waiter.Subscribe(&subscriber, handle0, SocketHandleWaiter::kReadable);

  EXPECT_CALL(waiter, AwaitSocketsReady(_, _))
      .WillOnce(
          Return(ByMove(std::vector<TestingSocketHandleWaiter::HandleWithFlags>{
              {handle0, SocketHandleWaiter::kReadable},  // Expected
              {handle1,
               SocketHandleWaiter::kReadable}  // Unexpected by subscription
          })));

  EXPECT_CALL(subscriber, ProcessReadyHandle(std::cref(handle0),
                                             SocketHandleWaiter::kReadable))
      .Times(1);
  EXPECT_CALL(subscriber, ProcessReadyHandle(std::cref(handle1), _))
      .Times(0);  // Should be ignored

  waiter.ProcessHandles(Clock::duration{0});
}

TEST_F(SocketHandleWaiterPosixInstanceTest, WriteNotProcessedIfNoPendingWrite) {
  int pipe_fds[2];
  CreatePipe(pipe_fds);
  SocketHandle write_end_handle(pipe_fds[1]);

  // Subscribe for write events on the write-end of the pipe.
  waiter_.Subscribe(&subscriber_, std::cref(write_end_handle),
                    SocketHandleWaiter::Flags::kWritable);

  // Simulate that the subscriber has no data to write.
  EXPECT_CALL(subscriber_, HasPendingWrite(std::cref(write_end_handle)))
      .WillRepeatedly(Return(false));

  // Expect ProcessReadyHandle NOT to be called with the kWritable flag.
  // Since we only subscribed for kWritable, and HasPendingWrite is false,
  // it should not be called at all for this handle's write event.
  EXPECT_CALL(subscriber_, ProcessReadyHandle(std::cref(write_end_handle), _))
      .Times(0);

  const Error result = waiter_.ProcessHandles(std::chrono::milliseconds(10));

  // If no FDs were ready (or ready but filtered out), ProcessHandles returns
  // kAgain.
  EXPECT_TRUE(result.ok() || result.code() == Error::Code::kAgain)
      << "ProcessHandles returned: " << result;

  waiter_.Unsubscribe(&subscriber_, std::cref(write_end_handle));
  ClosePipe(pipe_fds);
}

TEST_F(SocketHandleWaiterPosixInstanceTest,
       ReadProcessedWriteIgnoredIfNoPendingWrite) {
  int pipe_fds[2];
  CreatePipe(pipe_fds);
  SocketHandle read_end_handle(pipe_fds[0]);

  // Subscribe for read and write events on the read-end of the pipe.
  waiter_.Subscribe(&subscriber_, std::cref(read_end_handle),
                    SocketHandleWaiter::Flags::kReadable |
                        SocketHandleWaiter::Flags::kWritable);

  // Simulate that the subscriber has no data to write.
  EXPECT_CALL(subscriber_, HasPendingWrite(std::cref(read_end_handle)))
      .WillRepeatedly(Return(false));

  // Make the read-end readable.
  constexpr const char kTestBuf[] = "test";
  ASSERT_THAT(write(pipe_fds[1], kTestBuf, sizeof(kTestBuf) - 1),
              Gt(ssize_t{0}));

  // Expect ProcessReadyHandle to be called only with kReadable.
  EXPECT_CALL(subscriber_,
              ProcessReadyHandle(std::cref(read_end_handle),
                                 SocketHandleWaiter::Flags::kReadable))
      .Times(1);

  const Error result = waiter_.ProcessHandles(std::chrono::milliseconds(10));
  EXPECT_TRUE(result.ok()) << "ProcessHandles returned: " << result;

  std::array<uint8_t, 16> drain;
  read(pipe_fds[0], drain.data(), drain.size());
  waiter_.Unsubscribe(&subscriber_, std::cref(read_end_handle));
  ClosePipe(pipe_fds);
}

}  // namespace openscreen
