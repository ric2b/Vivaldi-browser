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

#ifndef FCP_TRACING_TEST_TRACING_SPAN_IMPL_H_
#define FCP_TRACING_TEST_TRACING_SPAN_IMPL_H_

#include "fcp/tracing/test_tracing_recorder_impl.h"
#include "fcp/tracing/tracing_span_impl.h"

namespace fcp {
namespace tracing_internal {

class TestTracingSpanImpl : public TracingSpanImpl {
 public:
  TestTracingSpanImpl(std::shared_ptr<TestTracingRecorderImpl> recorder,
                      TracingSpanId id);
  TestTracingSpanImpl(TestTracingRecorderImpl* recorder, TracingSpanId id);
  ~TestTracingSpanImpl() override;

  void TraceImpl(flatbuffers::DetachedBuffer&& buf,
                 const TracingTraitsBase& traits) override;
  TracingSpanRef Ref() override;

 private:
  TestTracingRecorderImpl* recorder_;

  // For non-root span the following keeps recorder alive, if set,
  // this holds the same value as recorder_. Since root span is owned directly
  // by the recorder we can't store shared_ptr for it here to avoid a loop.
  std::shared_ptr<TestTracingRecorderImpl> recorder_shared_ptr_;

  TracingSpanId id_;
};

}  // namespace tracing_internal
}  // namespace fcp

#endif  // FCP_TRACING_TEST_TRACING_SPAN_IMPL_H_
