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

#include "fcp/tracing/tracing_span_ref.h"

#include "fcp/tracing/tracing_recorder_impl.h"
#include "fcp/tracing/tracing_span_impl.h"

namespace fcp {

TracingSpanRef fcp::TracingSpanRef::Top() {
  tracing_internal::TracingSpanImpl* top =
      tracing_internal::TracingSpanImpl::Top();
  return top ? top->Ref()
             : tracing_internal::TracingRecorderImpl::GetCurrent()
                   ->GetRootSpan()
                   ->Ref();
}
}  // namespace fcp
