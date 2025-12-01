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

#include "fcp/tracing/text_tracing_span_impl.h"

#include <iostream>
#include <memory>
#include <utility>

#include "flatbuffers/minireflect.h"

namespace fcp {
namespace tracing_internal {

using flatbuffers::DetachedBuffer;

void TextTracingSpanImpl::TraceImpl(DetachedBuffer&& buf,
                                    const TracingTraitsBase& traits) {
  recorder_->TraceImpl(id_, std::move(buf), traits);
}

TextTracingSpanImpl::~TextTracingSpanImpl() {
  recorder_->EndSpan(id_, buf_text_.name, buf_text_.text_format);
}

TracingSpanRef TextTracingSpanImpl::Ref() {
  return TracingSpanRef(recorder_->shared_from_this(), id_);
}

}  // namespace tracing_internal
}  // namespace fcp
