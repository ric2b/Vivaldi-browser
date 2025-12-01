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

#ifndef FCP_TRACING_TEST_TRACING_RECORDER_IMPL_H_
#define FCP_TRACING_TEST_TRACING_RECORDER_IMPL_H_

#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "fcp/tracing/tracing_recorder_impl.h"

namespace fcp {
namespace tracing_internal {

class TestTracingSpanImpl;

class TestTracingRecorderImpl : public TracingRecorderImpl {
 public:
  // Allows listening for/handling traces as they appear.
  class TraceListener {
   public:
    virtual ~TraceListener() = default;
    // Called exactly once when the root span is created.
    virtual void OnRoot(TracingSpanId id, flatbuffers::DetachedBuffer data) = 0;
    // Called when a new span or event is created.
    // @param id Will only be called once per
    // unique ID (although it may be called multiple times with the same
    // parent span ID.) id
    virtual void OnTrace(TracingSpanId parent_id, TracingSpanId id,
                         flatbuffers::DetachedBuffer data) = 0;
  };
  explicit TestTracingRecorderImpl(TraceListener* trace_listener);
  ~TestTracingRecorderImpl() override;
  TracingSpanImpl* GetRootSpan() override;
  void TraceImpl(TracingSpanId id, flatbuffers::DetachedBuffer&& buf,
                 const TracingTraitsBase& traits) override;
  std::unique_ptr<TracingSpanImpl> CreateChildSpan(
      TracingSpanId parent_span_id, flatbuffers::DetachedBuffer&& buf,
      const TracingTraitsBase& traits) override;

 private:
  TraceListener* trace_listener_;
  std::unique_ptr<TestTracingSpanImpl> root_span_;
};

}  // namespace tracing_internal
}  // namespace fcp

#endif  // FCP_TRACING_TEST_TRACING_RECORDER_IMPL_H_
