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

#include "fcp/tracing/text_tracing_recorder_impl.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "absl/time/clock.h"
#include "fcp/tracing/text_tracing_span_impl.h"
#include "flatbuffers/minireflect.h"

namespace fcp::tracing_internal {

using std::endl;

TextTracingRecorderImpl::TextTracingRecorderImpl(const std::string& filename,
                                                 absl::TimeZone time_zone)
    : fstream_(filename), time_zone_(time_zone) {
  stream_ = &fstream_.value();
  root_span_ = std::make_unique<TextTracingSpanImpl>(this);
}

TextTracingRecorderImpl::TextTracingRecorderImpl(absl::TimeZone time_zone)
    : stream_(&std::cerr), time_zone_(time_zone) {
  root_span_ = std::make_unique<TextTracingSpanImpl>(this);
}

// TODO(team): Ensure traces from different threads are not interleaved.
void TextTracingRecorderImpl::TraceImpl(TracingSpanId id,
                                        flatbuffers::DetachedBuffer&& buf,
                                        const TracingTraitsBase& traits) {
  LogTime();
  *stream_ << id << ": " << TracingTraitsBase::SeverityString(traits.Severity())
           << " " << traits.Name() << traits.TextFormat(buf) << endl;
}

void TextTracingRecorderImpl::BeginSpan(TracingSpanId id,
                                        TracingSpanId parent_id,
                                        absl::string_view name,
                                        absl::string_view text_format) {
  LogSpan(/* begin = */ true, id, parent_id, name, text_format);
}

void TextTracingRecorderImpl::EndSpan(TracingSpanId id, absl::string_view name,
                                      absl::string_view text_format) {
  LogSpan(/* begin = */ false, id, TracingSpanId(0), name, text_format);
}

void TextTracingRecorderImpl::LogSpan(bool begin, TracingSpanId id,
                                      TracingSpanId parent_id,
                                      absl::string_view name,
                                      absl::string_view text_format) {
  LogTime();
  *stream_ << id << (begin ? ": BEGIN" : ": END");
  if (!name.empty()) {
    *stream_ << " " << name << " " << text_format;
  }
  if (begin && id != TracingSpanId(0)) {
    *stream_ << " parent: " << parent_id;
  }
  *stream_ << endl;
}

void TextTracingRecorderImpl::LogTime() {
  *stream_ << absl::FormatTime(absl::Now(), time_zone_) << " ";
}

TracingSpanImpl* TextTracingRecorderImpl::GetRootSpan() {
  return root_span_.get();
}

std::unique_ptr<TracingSpanImpl> TextTracingRecorderImpl::CreateChildSpan(
    TracingSpanId parent_span_id, flatbuffers::DetachedBuffer&& buf,
    const TracingTraitsBase& traits) {
  // NOTE: shared_from_this() is defined in a base class, so it returns
  // std::shared_ptr<TracingRecorderImpl> and we have to (safely) cast it here:
  auto shared_this =
      std::static_pointer_cast<TextTracingRecorderImpl>(shared_from_this());
  return std::make_unique<TextTracingSpanImpl>(
      shared_this, std::move(buf), traits, TracingSpanId::NextUniqueId(),
      parent_span_id);
}

TextTracingRecorderImpl::~TextTracingRecorderImpl() = default;

}  // namespace fcp::tracing_internal
