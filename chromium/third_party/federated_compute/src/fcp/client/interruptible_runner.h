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
#ifndef FCP_CLIENT_INTERRUPTIBLE_RUNNER_H_
#define FCP_CLIENT_INTERRUPTIBLE_RUNNER_H_

#include <functional>
#include <memory>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "fcp/base/future.h"
#include "fcp/base/scheduler.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/log_manager.h"

namespace fcp {
namespace client {

// An executor that runs operations in a background thread, polling a callback
// periodically whether to abort, and aborting the operation if necessary.
// This uses a single-threaded thread pool. During execution of an operation,
// should_abort is polled periodically (polling_period), and if it returns true,
// the abort_function supplied along with the operation is called. The operation
// is then expected to abort within graceful_shutdown_period. If not, a diag
// code is logged and we wait for some time longer (extended_shutdown_period),
// and if the operation still does not finish, the program exits.
// The destructor blocks until the background thread has become idle.
class InterruptibleRunner {
 public:
  // A struct used to group polling & timeout related parameters.
  struct TimingConfig {
    absl::Duration polling_period;
    absl::Duration graceful_shutdown_period;
    absl::Duration extended_shutdown_period;
  };

  // A struct used to group diagnostics related parameters.
  struct DiagnosticsConfig {
    ProdDiagCode interrupted;
    ProdDiagCode interrupt_timeout;
    ProdDiagCode interrupted_extended;
    ProdDiagCode interrupt_timeout_extended;
  };

  InterruptibleRunner(LogManager* log_manager,
                      std::function<bool()> should_abort,
                      const TimingConfig& timing_config,
                      const DiagnosticsConfig& diagnostics_config)
      : log_manager_(log_manager),
        should_abort_(should_abort),
        timing_config_(timing_config),
        diagnostics_config_(diagnostics_config) {
    thread_pool_ = fcp::CreateThreadPoolScheduler(1);
  }

  ~InterruptibleRunner() { thread_pool_->WaitUntilIdle(); }

  // Executes f() on a background thread, blocking until it is done. Returns
  // CANCELLED if the background thread was aborted, or a Status object from the
  // background thread on successful completion.
  absl::Status Run(std::function<absl::Status()> f,
                   std::function<void()> abort_function);

 private:
  absl::Status WaitUntilDone(fcp::thread::Future<absl::Status>&& run_future,
                             std::function<void()> abort_function);
  absl::Status Abort(fcp::thread::Future<absl::Status> run_future,
                     std::function<void()> abort_function);

  std::unique_ptr<Scheduler> thread_pool_;
  LogManager* const log_manager_;
  std::function<bool()> should_abort_;
  TimingConfig timing_config_;
  DiagnosticsConfig diagnostics_config_;
};

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_INTERRUPTIBLE_RUNNER_H_
