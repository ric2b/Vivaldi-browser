/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FCP_SECAGG_SHARED_ASYNC_ABORT_H_
#define FCP_SECAGG_SHARED_ASYNC_ABORT_H_

#include <atomic>
#include <string>

#include "absl/base/attributes.h"
#include "absl/synchronization/mutex.h"
#include "fcp/base/monitoring.h"

namespace fcp {
namespace secagg {

// A helper to allow polling for asynchronous aborts.  For ease of testing, this
// class does not manage its own atomic, which allows the atomic to be easily
// allocated on its own page.
//
// This class is thread-safe.
class AsyncAbort {
 public:
  explicit AsyncAbort(std::atomic<std::string*>* signal)
      : signal_(signal), mu_() {
    FCP_CHECK(signal_);
  }
  virtual ~AsyncAbort() = default;

  // AsyncAbort is neither copyable nor movable.
  AsyncAbort(const AsyncAbort&) = delete;
  AsyncAbort& operator=(const AsyncAbort&) = delete;

  // Signal an async. abort.  The abort message may not be reflected in
  // SecAggClient if it has already transitioned to a terminal state (aborted
  // or completed).
  void Abort(std::string message) {
    absl::WriterMutexLock _(&mu_);
    message_ = message;
    *signal_ = &message_;
  }

  // Returns whether the abort signal is raised.
  ABSL_MUST_USE_RESULT bool Signalled() const {
    return signal_->load(std::memory_order_relaxed);
  }

  // Returns the abort message specified by the abort signal.
  // If Signalled() returns false, the value is undefined.
  ABSL_MUST_USE_RESULT std::string Message() const {
    absl::ReaderMutexLock _(&mu_);
    return **signal_;
  }

  std::atomic<std::string*>* signal_;
  mutable absl::Mutex mu_;
  std::string message_ ABSL_GUARDED_BY(mu_);
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SHARED_ASYNC_ABORT_H_
