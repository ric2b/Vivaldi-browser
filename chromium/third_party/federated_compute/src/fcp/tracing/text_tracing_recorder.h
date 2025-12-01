// Copyright 2019 Google LLC
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

#ifndef FCP_TRACING_TEXT_TRACING_RECORDER_H_
#define FCP_TRACING_TEXT_TRACING_RECORDER_H_

#include <fstream>
#include <string>

#include "fcp/tracing/text_tracing_recorder_impl.h"
#include "fcp/tracing/tracing_recorder.h"

namespace fcp {

// Entry point for usage of basic tracing API implementation that simply writes
// to an output stream in human readable format.
class TextTracingRecorder : public TracingRecorder {
 public:
  // Constructs a TextTracingRecorder which will write events to the filestream
  // with a timestamp formatted for the provided timezone.
  explicit TextTracingRecorder(const std::string& filename,
                               absl::TimeZone time_zone)
      : impl_(std::make_shared<tracing_internal::TextTracingRecorderImpl>(
            filename, time_zone)) {}
  // Constructs a TextTracingRecorder which will write events to stderr
  // with a timestamp formatted for the provided timezone.
  explicit TextTracingRecorder(absl::TimeZone time_zone)
      : impl_(std::make_shared<tracing_internal::TextTracingRecorderImpl>(
            time_zone)) {}

  void InstallAsGlobal() override { impl_->InstallAsGlobal(); }
  void UninstallAsGlobal() override { impl_->UninstallAsGlobal(); }
  void InstallAsThreadLocal() override { impl_->InstallAsThreadLocal(); }
  void UninstallAsThreadLocal() override { impl_->UninstallAsThreadLocal(); }

 private:
  // The tracing recorder implementation shared between tracing spans.
  std::shared_ptr<tracing_internal::TextTracingRecorderImpl> impl_;
};

}  // namespace fcp

#endif  // FCP_TRACING_TEXT_TRACING_RECORDER_H_
