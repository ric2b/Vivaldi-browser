// Copyright 2021 Google LLC
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

#include "fcp/tracing/tracing_span_impl.h"

#include "fcp/tracing/tracing_recorder_impl.h"

namespace fcp {
namespace tracing_internal {

thread_local TracingSpanImpl* TracingSpanImpl::top_tracing_span_ = nullptr;

TracingSpanImpl* TracingSpanImpl::Top() { return top_tracing_span_; }

void TracingSpanImpl::Pop() {
  FCP_CHECK(top_tracing_span_ == this);
  top_tracing_span_ = prev_;
}

void TracingSpanImpl::Push() {
  FCP_CHECK(prev_ == nullptr);
  prev_ = top_tracing_span_;
  top_tracing_span_ = this;
}

}  // namespace tracing_internal
}  // namespace fcp
