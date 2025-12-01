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

#ifndef FCP_TRACING_TRACING_SPAN_REF_H_
#define FCP_TRACING_TRACING_SPAN_REF_H_

#include <memory>
#include <utility>

#include "fcp/tracing/tracing_span_id.h"

namespace fcp {

namespace tracing_internal {
class TracingRecorderImpl;
}

// Reference to a tracing span.
class TracingSpanRef {
  // Reference to the tracing recorder this reference was issued by:
  std::shared_ptr<fcp::tracing_internal::TracingRecorderImpl> recorder_;
  // Identifier of the span
  TracingSpanId span_id_;

 public:
  TracingSpanRef(
      std::shared_ptr<fcp::tracing_internal::TracingRecorderImpl> provider,
      TracingSpanId span_id)
      : recorder_(std::move(provider)), span_id_(span_id) {}

  std::shared_ptr<tracing_internal::TracingRecorderImpl> recorder() {
    return recorder_;
  }

  TracingSpanId span_id() const { return span_id_; }

  // Returns reference to the top tracing span on the current
  // thread/fiber. If there's no tracing span established, a
  // reference to the root span of global tracing recorder is returned.
  static TracingSpanRef Top();
};

}  // namespace fcp
#endif  // FCP_TRACING_TRACING_SPAN_REF_H_
