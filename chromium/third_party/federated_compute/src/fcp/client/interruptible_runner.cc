/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "fcp/client/interruptible_runner.h"

#include <cstdlib>
#include <functional>
#include <optional>
#include <utility>

#include "absl/status/status.h"
#include "fcp/base/future.h"
#include "fcp/base/monitoring.h"

namespace fcp {
namespace client {

absl::Status InterruptibleRunner::Run(std::function<absl::Status()> f,
                                      std::function<void()> abort_function) {
  // Check before even making the call.
  if (should_abort_()) {
    return absl::CancelledError("cancelled before posting callable");
  }
  fcp::thread::Future<absl::Status> run_future =
      fcp::thread::ScheduleFuture<absl::Status>(thread_pool_.get(), f);
  return WaitUntilDone(std::move(run_future), abort_function);
}

absl::Status InterruptibleRunner::WaitUntilDone(
    fcp::thread::Future<absl::Status>&& run_future,
    std::function<void()> abort_function) {
  // Wait until call is done, checking periodically whether we need to abort.
  while (true) {
    if (run_future.Wait(timing_config_.polling_period)) {
      std::optional<absl::Status> future_result = std::move(run_future).Take();
      // std::nullopt indicates the underlying promise was abandoned. To my
      // best knowledge this always indicates a programming error and hence
      // should result in a crash.
      FCP_CHECK(future_result != std::nullopt);
      return future_result.value();
    }

    if (should_abort_()) {
      return Abort(std::move(run_future), abort_function);
    }
  }
}

absl::Status InterruptibleRunner::Abort(
    fcp::thread::Future<absl::Status> run_future,
    std::function<void()> abort_function) {
  FCP_LOG(WARNING) << "Aborting run.";

  // Attempt to abort the ongoing call.
  abort_function();

  // Wait for at most the graceful shutdown period.
  if (run_future.Wait(timing_config_.graceful_shutdown_period)) {
    log_manager_->LogDiag(diagnostics_config_.interrupted);
    FCP_CHECK(std::move(run_future).Take() != std::nullopt);
    return absl::CancelledError("cancelled after graceful wait");
  }

  // Runnable failed to abort during the graceful shutdown period. Wait for
  // (possibly much) longer, because there's nothing much being
  // gained by returning with TF still running, but resources leak.
  log_manager_->LogDiag(diagnostics_config_.interrupt_timeout);
  if (run_future.Wait(timing_config_.extended_shutdown_period)) {
    log_manager_->LogDiag(diagnostics_config_.interrupted_extended);
    FCP_CHECK(std::move(run_future).Take() != std::nullopt);
    return absl::CancelledError("cancelled after extended wait");
  }

  // If even waiting for the long period didn't help, exit this process.
  // This is the worst case that will unfortunately happen - we hope the
  // logs above and below make it to a logging backend, allowing to narrow
  // the root cause down to particular models or builds; and the exit(0) should
  // avoid raising a crash dialog when training is running in a background
  // process. Nevertheless the goal should be to never reach this point.

  log_manager_->LogDiag(diagnostics_config_.interrupt_timeout_extended);
  exit(0);
}

}  // namespace client
}  // namespace fcp
