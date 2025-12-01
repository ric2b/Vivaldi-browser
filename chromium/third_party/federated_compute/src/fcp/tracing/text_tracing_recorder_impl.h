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

#ifndef FCP_TRACING_TEXT_TRACING_RECORDER_IMPL_H_
#define FCP_TRACING_TEXT_TRACING_RECORDER_IMPL_H_

#include <flatbuffers/flatbuffers.h>

#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "absl/time/time.h"
#include "fcp/tracing/tracing_recorder.h"
#include "fcp/tracing/tracing_recorder_impl.h"

namespace fcp {
namespace tracing_internal {

class TextTracingSpanImpl;
// Basic tracing API implementation that writes begin span, end span, and log
// events to a stream in a human-readable text format.
class TextTracingRecorderImpl : public TracingRecorderImpl {
 public:
  TextTracingRecorderImpl(const std::string& filename,
                          absl::TimeZone time_zone);

  ~TextTracingRecorderImpl() override;

  // Constructs a recorder implementation that writes tracing events to stderr.
  explicit TextTracingRecorderImpl(absl::TimeZone time_zone);

  // Creates a root span from which child tracing spans can be created.
  TracingSpanImpl* GetRootSpan() override;

  // Trace an event represented by the flatbuffer.
  void TraceImpl(TracingSpanId span_id, flatbuffers::DetachedBuffer&& buf,
                 const TracingTraitsBase& traits) override;

  // Log that the tracing span represented by the flatbuffer is starting.
  void BeginSpan(TracingSpanId id, TracingSpanId parent_id,
                 absl::string_view name, absl::string_view text_format);

  // Log that the tracing span represented by the provided flatbuf is finished.
  void EndSpan(TracingSpanId id, absl::string_view name,
               absl::string_view text_format);

  // Creates instance of the child tracing span with the parent span ID and
  // tracing data provided
  std::unique_ptr<TracingSpanImpl> CreateChildSpan(
      TracingSpanId parent_span_id, flatbuffers::DetachedBuffer&& buf,
      const TracingTraitsBase& traits) override;

 private:
  // Log a timestamp of the current time to the filestream.
  void LogTime();
  // Common method for logging begin or end of a span.
  void LogSpan(bool begin, TracingSpanId id, TracingSpanId parent_id,
               absl::string_view name, absl::string_view text_format);

  // File stream which is present only if this tracing recorder was constructed
  // to write to a file. Should not be written to directly. This field is
  // present because this class must own the filestream, since an instance of
  // this class can be shared by many tracing spans, some of which may outlive
  // the function that originally created the root tracing span.
  std::optional<std::ofstream> fstream_;

  // Pointer to an output stream to which tracing events are written.
  std::ostream* stream_;

  absl::TimeZone time_zone_;
  std::unique_ptr<TextTracingSpanImpl> root_span_;
};

}  // namespace tracing_internal
}  // namespace fcp

#endif  // FCP_TRACING_TEXT_TRACING_RECORDER_IMPL_H_
