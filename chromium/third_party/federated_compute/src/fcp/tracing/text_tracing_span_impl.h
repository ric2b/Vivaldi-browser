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

#ifndef FCP_TRACING_TEXT_TRACING_SPAN_IMPL_H_
#define FCP_TRACING_TEXT_TRACING_SPAN_IMPL_H_

#include <atomic>
#include <optional>
#include <string>
#include <utility>

#include "fcp/tracing/text_tracing_recorder_impl.h"
#include "fcp/tracing/tracing_span_impl.h"
#include "flatbuffers/flatbuffers.h"

namespace fcp {
namespace tracing_internal {

// Basic tracing span implementation that owns the flatbuf representing the
// current span.
class TextTracingSpanImpl : public TracingSpanImpl {
 public:
  // Constructs a TextTracingSpanImpl for a child span from a serialized flatbuf
  // and TracingTraitsBase which provides more context about the flatbuf table.
  TextTracingSpanImpl(std::shared_ptr<TextTracingRecorderImpl> recorder,
                      const flatbuffers::DetachedBuffer& buf,
                      const TracingTraitsBase& traits, TracingSpanId id,
                      TracingSpanId parent_id)
      : id_(id),
        recorder_shared_ptr_(std::move(recorder)),
        buf_text_{traits.Name(), traits.TextFormat(buf)} {
    recorder_ = recorder_shared_ptr_.get();
    recorder_->BeginSpan(id, parent_id, buf_text_.name, buf_text_.text_format);
  }

  // Constructs a TextTracingSpanImpl for root span.
  explicit TextTracingSpanImpl(TextTracingRecorderImpl* recorder)
      : id_(0), recorder_(recorder), buf_text_{} {
    recorder_->BeginSpan(TracingSpanId(0), TracingSpanId(0), buf_text_.name,
                         buf_text_.text_format);
  }
  ~TextTracingSpanImpl() override;

  // Logs an event in the current tracing span.
  void TraceImpl(flatbuffers::DetachedBuffer&& buf,
                 const TracingTraitsBase& traits) override;

  TracingSpanRef Ref() override;

 private:
  struct SpanText {
    std::string name;
    std::string text_format;
  };
  TracingSpanId id_;
  TextTracingRecorderImpl* recorder_;

  // For non-root span the following keeps recorder alive, if set,
  // this holds the same value as recorder_. Since root span is owned directly
  // by the recorder we can't store shared_ptr for it here to avoid a loop.
  std::shared_ptr<TextTracingRecorderImpl> recorder_shared_ptr_;

  // Human readable text describing the flatbuffer representing the current
  // span; empty if this is the root span.
  SpanText buf_text_;
};

}  // namespace tracing_internal
}  // namespace fcp

#endif  // FCP_TRACING_TEXT_TRACING_SPAN_IMPL_H_
