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

#ifndef FCP_TRACING_TRACING_SPAN_IMPL_H_
#define FCP_TRACING_TRACING_SPAN_IMPL_H_

#include "fcp/base/monitoring.h"
#include "fcp/tracing/tracing_span_ref.h"
#include "fcp/tracing/tracing_traits.h"
#include "flatbuffers/flatbuffers.h"

namespace fcp {
namespace tracing_internal {

class TracingSpanImpl {
 public:
  // TracingSpanImpl is neither copyable nor movable:
  TracingSpanImpl(const TracingSpanImpl&) = delete;
  TracingSpanImpl& operator=(const TracingSpanImpl&) = delete;
  // Destructor closes the span
  virtual ~TracingSpanImpl() = default;
  // Internal logging implementation, to be used by tracing recorder only.
  virtual void TraceImpl(flatbuffers::DetachedBuffer&& buf,
                         const TracingTraitsBase& traits) = 0;

  // Pushes current tracing span to be the top one on the current thread/fiber:
  void Push();
  // Pops current tracing span
  void Pop();
  // Returns top tracing span for the current thread/fiber
  static TracingSpanImpl* Top();

  // Returns reference to this tracing span
  virtual TracingSpanRef Ref() = 0;

 protected:
  // TracingSpanImpl can't be directly constructed, use CreateChild():
  TracingSpanImpl() = default;

 private:
  // Optional pointer to the previous tracing span which was a Top() one before
  // this span was pushed.
  // This is used so we can restore the top one with Pop().
  // NOTE: while this is frequently points to the parent span, it doesn't have
  // to be the parent span, since a span might be constructed with arbitrary
  // parent, which doesn't have to be the current Top() one. Example: when a new
  // fiber is started the parent is on a different stack.
  TracingSpanImpl* prev_ = nullptr;

  // TODO(team): this assumes 1:1 fiber-thread relationship, use FCB:
  thread_local static tracing_internal::TracingSpanImpl* top_tracing_span_;
};

}  // namespace tracing_internal
}  // namespace fcp

#endif  // FCP_TRACING_TRACING_SPAN_IMPL_H_
