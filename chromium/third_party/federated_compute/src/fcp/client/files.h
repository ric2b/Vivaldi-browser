/*
 * Copyright 2019 Google LLC
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
#ifndef FCP_CLIENT_FILES_H_
#define FCP_CLIENT_FILES_H_

#include <string>

#include "absl/status/statusor.h"

namespace fcp {
namespace client {

// An interface used by the plan engine for platform-dependent file system
// access.
class Files {
 public:
  virtual ~Files() = default;

  // Creates a temporary file. The runtime environment (e.g. operating system)
  // is expected to clean up these files if necessary, i.e. the engine is not
  // responsible for their deletion (but may chose to do so).
  // On success, returns a file path.
  // On error, returns
  // - INTERNAL - unexpected error.
  // - INVALID_ARGUMENT - on "expected" errors such as I/O issues.
  virtual absl::StatusOr<std::string> CreateTempFile(
      const std::string& prefix, const std::string& suffix) = 0;
};

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_FILES_H_
