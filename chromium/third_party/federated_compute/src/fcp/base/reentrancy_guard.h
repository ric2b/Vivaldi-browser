/*
 * Copyright 2021 Google LLC
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

#ifndef FCP_BASE_REENTRANCY_GUARD_H_
#define FCP_BASE_REENTRANCY_GUARD_H_

#include <atomic>

#include "fcp/base/monitoring.h"

namespace fcp {

/**
 * ReentrancyGuard class is used to enforce strictly sequential calling pattern.
 * Usage pattern:
 *
 * Status Method(...) {
 *   ReentrancyGuard guard;
 *   FCP_RETURN_IF_ERROR(guard.Check(&in_use_));
 *
 *   // The rest of the method body...
 * }
 *
 * in_use_ above is std::atomic<bool> value stored in the object which methods
 * are enforced.
 */
class ReentrancyGuard {
 public:
  Status Check(std::atomic<bool>* in_use) {
    FCP_CHECK(in_use != nullptr);
    bool expected_value = false;
    if (!in_use->compare_exchange_strong(expected_value, true)) {
      return FCP_STATUS(FAILED_PRECONDITION)
             << "Concurrent method calls detected";
    }

    in_use_ = in_use;
    return FCP_STATUS(OK);
  }

  ~ReentrancyGuard() {
    if (in_use_ != nullptr) {
      in_use_->store(false);
    }
  }

 private:
  // Pointer to atomic boolean value which is owned by the object which methods
  // are guarded against reentrancy. This value is set to true when inside
  // a method call; otherwise false.
  // Note: std::atomic is used here rather than Mutex is emphasise non-blocking
  // nature of the implementation. The purpose in_use_ is only to check against
  // reentrancy rather than synchronization.
  std::atomic<bool>* in_use_ = nullptr;
};

}  // namespace fcp

#endif  // FCP_BASE_REENTRANCY_GUARD_H_
