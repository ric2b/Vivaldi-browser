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

#ifndef FCP_BASE_PROCESS_UNIQUE_ID_H_
#define FCP_BASE_PROCESS_UNIQUE_ID_H_

#include <atomic>

namespace fcp {

// Threadsafe class for creating IDs that are unique per-process.
class ProcessUniqueId {
 public:
  // Threadsafe method for getting a new unique ID, intended to be cheap.
  static ProcessUniqueId Next();
  uint64_t value() { return value_; }

 private:
  explicit constexpr ProcessUniqueId(uint64_t value) : value_(value) {}
  // Value of the unique ID.
  uint64_t value_;
};

}  // namespace fcp

#endif  // FCP_BASE_PROCESS_UNIQUE_ID_H_
