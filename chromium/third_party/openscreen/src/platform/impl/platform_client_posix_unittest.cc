// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/platform_client_posix.h"

#include <chrono>
#include <memory>
#include <thread>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/time.h"
#include "platform/impl/task_runner.h"
#include "platform/impl/tls_data_router_posix.h"
#include "platform/impl/udp_socket_reader_posix.h"
#include "platform/test/fake_clock.h"

namespace openscreen {

using ::testing::_;
using ::testing::Return;

// Default timeout for operations in tests.
const Clock::duration kDefaultTestTimeout = std::chrono::milliseconds(10);

// Mock for TaskRunner to inject and verify interactions.
class MockTaskRunnerImpl final : public TaskRunnerImpl {
 public:
  explicit MockTaskRunnerImpl(ClockNowFunctionPtr now_function)
      : TaskRunnerImpl(now_function) {}
  ~MockTaskRunnerImpl() override = default;

  MOCK_METHOD(void, PostPackagedTask, (TaskRunner::Task task), (override));
  MOCK_METHOD(void,
              PostPackagedTaskWithDelay,
              (TaskRunner::Task task, Clock::duration delay),
              (override));
  MOCK_METHOD(bool, IsRunningOnTaskRunner, (), (override));
  MOCK_METHOD(void, RunUntilStopped, (), (override));
  MOCK_METHOD(void, RequestStopSoon, (), (override));
};

class PlatformClientPosixTest : public ::testing::Test {
 protected:
  PlatformClientPosixTest() {
    // Ensure no instance exists before each test.
    if (PlatformClientPosix::GetInstance()) {
      PlatformClientPosix::ShutDown();
    }
    OSP_CHECK_EQ(PlatformClientPosix::GetInstance(), nullptr);
  }

  ~PlatformClientPosixTest() {
    // Ensure cleanup if a test fails to call ShutDown.
    if (PlatformClientPosix::GetInstance()) {
      PlatformClientPosix::ShutDown();
    }
    OSP_CHECK_EQ(PlatformClientPosix::GetInstance(), nullptr);
  }
};

TEST_F(PlatformClientPosixTest, CreateAndShutdown_DefaultTaskRunner) {
  EXPECT_EQ(PlatformClientPosix::GetInstance(), nullptr);

  PlatformClientPosix::Create(kDefaultTestTimeout);
  PlatformClientPosix* instance = PlatformClientPosix::GetInstance();
  EXPECT_NE(instance, nullptr);
  EXPECT_NE(&instance->GetTaskRunner(), nullptr);

  PlatformClientPosix::ShutDown();
  EXPECT_EQ(PlatformClientPosix::GetInstance(), nullptr);
}

TEST_F(PlatformClientPosixTest, CreateAndShutdown_ProvidedTaskRunner) {
  EXPECT_EQ(PlatformClientPosix::GetInstance(), nullptr);
  auto mock_task_runner = std::make_unique<MockTaskRunnerImpl>(&FakeClock::now);

  // When the PlatformClientPosix is shut down, it will request its TaskRunner
  // to stop.
  EXPECT_CALL(*mock_task_runner, RequestStopSoon()).Times(1);
  EXPECT_CALL(*mock_task_runner, RunUntilStopped()).Times(0);

  const TaskRunner* mock_task_runner_ptr = mock_task_runner.get();
  PlatformClientPosix::Create(kDefaultTestTimeout, std::move(mock_task_runner));
  PlatformClientPosix* instance = PlatformClientPosix::GetInstance();
  EXPECT_NE(instance, nullptr);

  // Check that GetTaskRunner returns a reference to our mock.
  // Since GetTaskRunner() returns TaskRunner&, and we passed TaskRunnerImpl*,
  // we can check if the address matches.
  EXPECT_EQ(&instance->GetTaskRunner(), mock_task_runner_ptr);

  PlatformClientPosix::ShutDown();
  EXPECT_EQ(PlatformClientPosix::GetInstance(), nullptr);
}

TEST_F(PlatformClientPosixTest,
       GetInstance_ReturnsNullBeforeCreateAndAfterShutdown) {
  EXPECT_EQ(PlatformClientPosix::GetInstance(), nullptr);

  PlatformClientPosix::Create(kDefaultTestTimeout);
  EXPECT_NE(PlatformClientPosix::GetInstance(), nullptr);

  PlatformClientPosix::ShutDown();
  EXPECT_EQ(PlatformClientPosix::GetInstance(), nullptr);
}

TEST_F(PlatformClientPosixTest, ComponentInitialization_UdpSocketReader) {
  PlatformClientPosix::Create(kDefaultTestTimeout);
  PlatformClientPosix* instance = PlatformClientPosix::GetInstance();
  EXPECT_NE(instance, nullptr);

  // First call should initialize the UdpSocketReader.
  UdpSocketReaderPosix* reader1 = instance->udp_socket_reader();
  EXPECT_NE(reader1, nullptr);

  // Second call should return the same instance.
  UdpSocketReaderPosix* reader2 = instance->udp_socket_reader();
  EXPECT_EQ(reader1, reader2);

  PlatformClientPosix::ShutDown();
}

TEST_F(PlatformClientPosixTest, ComponentInitialization_TlsDataRouter) {
  PlatformClientPosix::Create(kDefaultTestTimeout);
  PlatformClientPosix* instance = PlatformClientPosix::GetInstance();
  EXPECT_NE(instance, nullptr);

  // First call should initialize the TlsDataRouter.
  TlsDataRouterPosix* router1 = instance->tls_data_router();
  EXPECT_NE(router1, nullptr);

  // Second call should return the same instance.
  TlsDataRouterPosix* router2 = instance->tls_data_router();
  EXPECT_EQ(router1, router2);

  PlatformClientPosix::ShutDown();
}

}  // namespace openscreen
