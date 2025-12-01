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
#ifndef FCP_CLIENT_ENGINE_CACHING_ERROR_REPORTER_H_
#define FCP_CLIENT_ENGINE_CACHING_ERROR_REPORTER_H_

#include <cstdarg>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "tensorflow/lite/core/api/error_reporter.h"

namespace fcp {
namespace client {
namespace engine {

// This implementation of ErrorReporter stores all the error messages.
class CachingErrorReporter : public tflite::ErrorReporter {
 public:
  int Report(const char* format, va_list args) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::string GetFirstErrorMessage() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  absl::Mutex mutex_;
  static constexpr int kBufferSize = 1024;
  // There could be more than one error messages so we store all of them.
  std::vector<std::string> error_messages_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace engine
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_ENGINE_CACHING_ERROR_REPORTER_H_
