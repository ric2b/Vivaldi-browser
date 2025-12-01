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

#include "fcp/tracing/test_tracing_span_impl.h"

#include <memory>
#include <utility>

namespace fcp {
namespace tracing_internal {

using flatbuffers::DetachedBuffer;

TestTracingSpanImpl::TestTracingSpanImpl(
    std::shared_ptr<TestTracingRecorderImpl> recorder, TracingSpanId id)
    : recorder_shared_ptr_(std::move(recorder)), id_(id) {
  recorder_ = recorder_shared_ptr_.get();
}

TestTracingSpanImpl::TestTracingSpanImpl(TestTracingRecorderImpl* recorder,
                                         TracingSpanId id)
    : recorder_(recorder), id_(id) {}

void TestTracingSpanImpl::TraceImpl(DetachedBuffer&& buf,
                                    const TracingTraitsBase& traits) {
  recorder_->TraceImpl(id_, std::move(buf), traits);
}

TestTracingSpanImpl::~TestTracingSpanImpl() = default;

TracingSpanRef TestTracingSpanImpl::Ref() {
  return TracingSpanRef(recorder_->shared_from_this(), id_);
}
}  // namespace tracing_internal
}  // namespace fcp
