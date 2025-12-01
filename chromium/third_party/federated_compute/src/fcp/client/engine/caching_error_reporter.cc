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
#include "fcp/client/engine/caching_error_reporter.h"

#include <stdio.h>

#include <cstdarg>
#include <string>

#include "absl/synchronization/mutex.h"

namespace fcp {
namespace client {
namespace engine {

int CachingErrorReporter::Report(const char* format, va_list args) {
  absl::MutexLock lock(&mutex_);
  char error_msg[kBufferSize];
  int num_characters = vsnprintf(error_msg, kBufferSize, format, args);
  if (num_characters >= 0) {
    error_messages_.push_back(std::string(error_msg));
  } else {
    // If num_characters is below zero, we can't trust the created string, so we
    // push an "Unknown error" to the stored error messages.
    // We don't want to crash here, because the TFLite execution will be
    // terminated soon.
    error_messages_.push_back("Unknown error.");
  }
  return num_characters;
}

std::string CachingErrorReporter::GetFirstErrorMessage() {
  absl::MutexLock lock(&mutex_);
  if (error_messages_.empty()) {
    return "";
  } else {
    return error_messages_[0];
  }
}

}  // namespace engine
}  // namespace client
}  // namespace fcp
