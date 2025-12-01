// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FCP_TRACING_SCOPED_TRACING_RECORDER_H_
#define FCP_TRACING_SCOPED_TRACING_RECORDER_H_

#include "fcp/tracing/tracing_recorder.h"

namespace fcp {

// This is an utility class that installs a specified tracing recorder as
// thread local and uninstalls it automatically when going out of scope.
class ScopedTracingRecorder {
 public:
  explicit ScopedTracingRecorder(TracingRecorder* tracing_recorder)
      : tracing_recorder_(tracing_recorder) {
    tracing_recorder_->InstallAsThreadLocal();
  }

  ~ScopedTracingRecorder() { tracing_recorder_->UninstallAsThreadLocal(); }

  // This class isn't copyable or moveable and can't be created via
  // new operator.
  ScopedTracingRecorder(const ScopedTracingRecorder& other) = delete;
  ScopedTracingRecorder& operator=(const ScopedTracingRecorder& other) = delete;
  void* operator new(std::size_t) = delete;
  void* operator new[](std::size_t) = delete;

 private:
  TracingRecorder* tracing_recorder_;
};

}  // namespace fcp

#endif  // FCP_TRACING_SCOPED_TRACING_RECORDER_H_
