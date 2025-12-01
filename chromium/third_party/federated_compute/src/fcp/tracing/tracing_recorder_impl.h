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

#ifndef FCP_TRACING_TRACING_RECORDER_IMPL_H_
#define FCP_TRACING_TRACING_RECORDER_IMPL_H_

#include <memory>

#include "fcp/tracing/tracing_span_impl.h"

namespace fcp {
namespace tracing_internal {

class TracingRecorderImpl
    : public std::enable_shared_from_this<TracingRecorderImpl> {
 public:
  TracingRecorderImpl() = default;
  virtual ~TracingRecorderImpl();

  // TracingRecorderImpl is neither copyable nor movable.
  TracingRecorderImpl(const TracingRecorderImpl&) = delete;
  TracingRecorderImpl& operator=(const TracingRecorderImpl&) = delete;

  // Trace an event represented by the flatbuffer.
  virtual void TraceImpl(TracingSpanId span_id,
                         flatbuffers::DetachedBuffer&& buf,
                         const TracingTraitsBase& traits) = 0;
  virtual TracingSpanImpl* GetRootSpan() = 0;

  // Creates child span.
  virtual std::unique_ptr<TracingSpanImpl> CreateChildSpan(
      TracingSpanId parent_span_id, flatbuffers::DetachedBuffer&& buf,
      const TracingTraitsBase& traits) = 0;

  // Installs this tracing recorder as global singleton instance.
  void InstallAsGlobal();

  // Uninstalls this tracing recorder as global instance. Automatically
  // called upon destruction.
  void UninstallAsGlobal();

  // Installs this tracing recorder as thread local singleton instance.
  void InstallAsThreadLocal();

  // Uninstalls this tracing recorder as thread local singleton instance.
  void UninstallAsThreadLocal();

  // Gets the current thread local tracing recorder if set; otherwise gets
  // the current global tracing recorder.
  static std::shared_ptr<TracingRecorderImpl> GetCurrent();
  bool is_global_ = false;
};

}  // namespace tracing_internal
}  // namespace fcp

#endif  // FCP_TRACING_TRACING_RECORDER_IMPL_H_
