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

#include "fcp/tracing/test_tracing_recorder_impl.h"

#include <memory>
#include <utility>

#include "fcp/tracing/test_tracing_span_impl.h"

namespace fcp {
namespace tracing_internal {

using flatbuffers::DetachedBuffer;
using flatbuffers::FlatBufferBuilder;

DetachedBuffer EmptyFlatBuffer() {
  FlatBufferBuilder fbb;
  fbb.Finish(fbb.CreateString("ROOT"), "ROOT");
  return fbb.Release();
}

TestTracingRecorderImpl::TestTracingRecorderImpl(TraceListener* trace_listener)
    : trace_listener_(trace_listener),
      root_span_(
          std::make_unique<TestTracingSpanImpl>(this, TracingSpanId(0))) {
  trace_listener_->OnRoot(TracingSpanId(0), EmptyFlatBuffer());
}

void TestTracingRecorderImpl::TraceImpl(TracingSpanId id, DetachedBuffer&& buf,
                                        const TracingTraitsBase& traits) {
  TracingSpanId new_id = TracingSpanId::NextUniqueId();
  trace_listener_->OnTrace(id, new_id, std::move(buf));
}

TracingSpanImpl* TestTracingRecorderImpl::GetRootSpan() {
  return root_span_.get();
}

std::unique_ptr<TracingSpanImpl> TestTracingRecorderImpl::CreateChildSpan(
    TracingSpanId parent_span_id, flatbuffers::DetachedBuffer&& buf,
    const TracingTraitsBase& traits) {
  TracingSpanId new_id = TracingSpanId::NextUniqueId();
  trace_listener_->OnTrace(parent_span_id, new_id, std::move(buf));
  // NOTE: shared_from_this() is defined in a base class, so it returns
  // std::shared_ptr<TracingRecorderImpl> and we have to (safely) cast it here:
  auto shared_this =
      std::static_pointer_cast<TestTracingRecorderImpl>(shared_from_this());
  return std::make_unique<TestTracingSpanImpl>(shared_this, new_id);
}

TestTracingRecorderImpl::~TestTracingRecorderImpl() = default;

}  // namespace tracing_internal
}  // namespace fcp
