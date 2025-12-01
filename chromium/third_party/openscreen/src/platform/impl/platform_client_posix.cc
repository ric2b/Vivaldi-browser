// Copyright (c) 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/platform_client_posix.h"

#include <chrono>
#include <functional>
#include <utility>
#include <vector>

#include "platform/base/trivial_clock_traits.h"
#include "platform/impl/udp_socket_reader_posix.h"

namespace openscreen {

using clock_operators::operator<<;

// static
PlatformClientPosix* PlatformClientPosix::instance_ = nullptr;

// static
void PlatformClientPosix::Create(Clock::duration networking_operation_timeout,
                                 std::unique_ptr<TaskRunnerImpl> task_runner) {
  SetInstance(new PlatformClientPosix(networking_operation_timeout,
                                      std::move(task_runner)));
}

// static
void PlatformClientPosix::Create(Clock::duration networking_operation_timeout) {
  SetInstance(new PlatformClientPosix(networking_operation_timeout));
}

// static
void PlatformClientPosix::ShutDown() {
  OSP_CHECK(instance_);
  delete instance_;
  instance_ = nullptr;
}

TlsDataRouterPosix* PlatformClientPosix::tls_data_router() {
  std::call_once(tls_data_router_initialization_, [this]() {
    tls_data_router_ =
        std::make_unique<TlsDataRouterPosix>(socket_handle_waiter());
    tls_data_router_created_.store(true);
  });
  return tls_data_router_.get();
}

UdpSocketReaderPosix* PlatformClientPosix::udp_socket_reader() {
  std::call_once(udp_socket_reader_initialization_, [this]() {
    udp_socket_reader_ =
        std::make_unique<UdpSocketReaderPosix>(*socket_handle_waiter());
  });
  return udp_socket_reader_.get();
}

TaskRunner& PlatformClientPosix::GetTaskRunner() {
  return *task_runner_;
}

PlatformClientPosix::~PlatformClientPosix() {
  OSP_DVLOG << "Shutting down the Task Runner...";
  task_runner_->RequestStopSoon();
  if (task_runner_thread_ && task_runner_thread_->joinable()) {
    task_runner_thread_->join();
    OSP_DVLOG << "\tTask Runner shutdown complete!";
  }

  OSP_DVLOG << "Shutting down network operations...";
  networking_loop_running_.store(false);
  networking_loop_thread_.join();
  OSP_DVLOG << "\tNetwork operation shutdown complete!";
}

// static
void PlatformClientPosix::SetInstance(PlatformClientPosix* instance) {
  OSP_CHECK(!instance_);
  instance_ = instance;
}

PlatformClientPosix::PlatformClientPosix(
    Clock::duration networking_operation_timeout)
    : task_runner_(new TaskRunnerImpl(Clock::now)),
      networking_loop_timeout_(networking_operation_timeout),
      networking_loop_thread_(&PlatformClientPosix::RunNetworkLoopUntilStopped,
                              this),
      task_runner_thread_(
          std::thread(&TaskRunnerImpl::RunUntilStopped, task_runner_.get())) {}

PlatformClientPosix::PlatformClientPosix(
    Clock::duration networking_operation_timeout,
    std::unique_ptr<TaskRunnerImpl> task_runner)
    : task_runner_(std::move(task_runner)),
      networking_loop_timeout_(networking_operation_timeout),
      networking_loop_thread_(&PlatformClientPosix::RunNetworkLoopUntilStopped,
                              this) {}

SocketHandleWaiterPosix* PlatformClientPosix::socket_handle_waiter() {
  std::call_once(waiter_initialization_, [this]() {
    waiter_ = std::make_unique<SocketHandleWaiterPosix>(&Clock::now);
    waiter_created_.store(true);
  });
  return waiter_.get();
}

void PlatformClientPosix::RunNetworkLoopUntilStopped() {
#if OSP_DCHECK_IS_ON()
  Clock::time_point last_time = Clock::now();
  int iterations = 0;
#endif
  while (networking_loop_running_.load()) {
#if OSP_DCHECK_IS_ON()
    ++iterations;
    const Clock::time_point current_time = Clock::now();
    const Clock::duration delta = current_time - last_time;
    if (delta > std::chrono::seconds(1)) {
      OSP_DCHECK_GT(iterations, 0);
      OSP_VLOG << "network loop execution time averaged "
               << (delta / iterations) << " over the last second.";
      last_time = current_time;
      iterations = 0;
    }
#endif
    if (!waiter_created_.load()) {
      std::this_thread::sleep_for(networking_loop_timeout_);
      continue;
    }
    const Error process_error =
        socket_handle_waiter()->ProcessHandles(networking_loop_timeout_);

    // We may receive an "again" error code if there were no sockets to process.
    if (process_error.code() == Error::Code::kAgain) {
      std::this_thread::sleep_for(networking_loop_timeout_);
      continue;

      // If there is a socket error it should be handled elsewhere. Just log
      // the error here.
    } else if (!process_error.ok()) {
      OSP_LOG_ERROR << "error occurred while processing handles. error="
                    << process_error;
    }
  }
}

}  // namespace openscreen
