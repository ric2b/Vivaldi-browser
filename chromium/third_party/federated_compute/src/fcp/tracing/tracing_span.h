// Copyright 2020 Google LLC
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

#ifndef FCP_TRACING_TRACING_SPAN_H_
#define FCP_TRACING_TRACING_SPAN_H_

#include <memory>
#include <string>
#include <utility>

#include "fcp/base/error.h"
#include "fcp/tracing/tracing_recorder_impl.h"
#include "fcp/tracing/tracing_span_impl.h"
#include "fcp/tracing/tracing_span_ref.h"
#include "fcp/tracing/tracing_traits.h"
#include "flatbuffers/flatbuffers.h"

namespace fcp {

namespace tracing_internal {

template <class FlatBufferTable>
void AssertIsNotSpan() {
  static_assert(TracingTraits<FlatBufferTable>::kIsSpan == false,
                "Trace can only be called on an event table, not a span. To "
                "convert the table to an event remove 'span' from the table "
                "attributes.");
}

template <class FlatBufferTable>
void AssertIsError() {
  static_assert(
      TracingTraits<FlatBufferTable>::kSeverity == TracingSeverity::kError,
      "TraceError can only be called on table of severity Error. To "
      "convert the table to error severity add 'error' to the "
      "table attributes.");
}

// Generic method to build FlatBuff tables from a set of args.
template <class FlatBufferTable, class... Arg>
flatbuffers::DetachedBuffer BuildFlatBuffer(Arg&&... args) {
  flatbuffers::FlatBufferBuilder fbb;
  flatbuffers::Offset<FlatBufferTable> root =
      TracingTraits<FlatBufferTable>::Create(std::forward<Arg>(args)..., &fbb);
  constexpr TracingTag tag = TracingTraits<FlatBufferTable>::kTag;
  constexpr char signature[5] = {tag.data[0], tag.data[1], tag.data[2],
                                 tag.data[3], 0};
  fbb.Finish(root, signature);
  return fbb.Release();
}

/*
 * Un-templatized version of the tracing span which stores the raw
 * definition of the tracing span and returns a reference to it.
 *
 * This class should not/cannot be publicly initialized, instead it should be
 * used as a base class for both UnscopedTracingSpan and TracingSpan.
 *
 * For Java based tracing spans, all native spans will be cast to this common
 * structure to pass along the JNI boundary.
 */
class TracingSpanBase {
 public:
  // Returns a reference to this span.
  inline TracingSpanRef Ref() const { return impl_->Ref(); }
  virtual ~TracingSpanBase() = default;

 protected:
  explicit TracingSpanBase() = default;
  // Set by the child classes after an instance of TracingSpanImpl is created
  // from the flatbuf definitions.
  std::unique_ptr<TracingSpanImpl> impl_;
};

}  // namespace tracing_internal

// Unscoped tracing span carries tracing context for some logical activity.
// The primary purpose of this class is to keep the tracing context of a
// long-running asynchronous activity, typically associated with the lifetime of
// some long-lived object.
//
// This span is not scoped to a block/function and need not necessarily be
// instantiated as a local variable on the stack. It is OK to initialise this
// span on a heap and/or as a field member of a class. For scoped variant,
// prefer using TracingSpan instead.
//
// To record a Trace within an UnscopedTracingSpan, the caller must explicitly
// pass a reference to a span e.g Trace<FlatBuff>(span, args...).
//
// The class is NOT thread-safe, but OK to use from different threads if invoked
// sequentially (i.e. with external synchronisation providing sequential
// consistency memory ordering).
//
// Recommended usage is to create new child span for every sub-activity or
// operation.
template <class FlatBufferTable>
class UnscopedTracingSpan : public tracing_internal::TracingSpanBase {
 public:
  // UnscopedTracingSpan is neither copyable nor movable.
  UnscopedTracingSpan(const UnscopedTracingSpan&) = delete;
  UnscopedTracingSpan& operator=(const UnscopedTracingSpan&) = delete;
  // Public constructors allow creating new tracing span.
  // A parent span reference can be optionally provided as a first argument.
  // By default current span is used as a parent.
  template <class... Arg>
  explicit UnscopedTracingSpan(Arg&&... args);
  template <class... Arg>
  explicit UnscopedTracingSpan(TracingSpanRef parent, Arg&&... args);

 private:
  template <class... Arg>
  void Create(TracingSpanRef parent, Arg&&... args);
};

template <class FlatBufferTable>
template <class... Arg>
UnscopedTracingSpan<FlatBufferTable>::UnscopedTracingSpan(Arg&&... args) {
  Create(TracingSpanRef::Top(), std::forward<Arg>(args)...);
}

template <class FlatBufferTable>
template <class... Arg>
UnscopedTracingSpan<FlatBufferTable>::UnscopedTracingSpan(TracingSpanRef parent,
                                                          Arg&&... args) {
  Create(parent, std::forward<Arg>(args)...);
}

template <class FlatBufferTable>
template <class... Arg>
void UnscopedTracingSpan<FlatBufferTable>::Create(TracingSpanRef parent,
                                                  Arg&&... args) {
  static_assert(
      TracingTraits<FlatBufferTable>::kIsSpan == true,
      "UnscopedTracingSpan can only be created from a span table, not "
      "an event. To convert the table to an span add 'span' "
      "to the table attributes.");
  flatbuffers::DetachedBuffer trace_data =
      tracing_internal::BuildFlatBuffer<FlatBufferTable>(
          std::forward<Arg>(args)...);
  impl_ = parent.recorder()->CreateChildSpan(parent.span_id(),
                                             std::move(trace_data),
                                             TracingTraits<FlatBufferTable>());
}

// Tracing span, carrying abstract tracing context for some logical activity
// performed by the application. Provides ability to log various events
// which are automatically associated with such a context, thus allowing logging
// essential information only.
//
// This class uses RAII-style mechanism of entering/existing the tracing span
// for the duration of a scoped block or a function, this class must be
// instantiated as a local variable on the stack, in a similar manner as
// std::lock_guard.
//
// The class is NOT thread-safe, but OK to use from different threads if invoked
// sequentially (i.e. with external synchronisation providing sequential
// consistency memory ordering).
//
// Recommended usage is to create new child span for every sub-activity or
// operation.
//
// For a more general variant that is not necessarily tied to a scoped block,
// prefer using UnscopedTracingSpan directly.
template <class FlatBufferTable>
class TracingSpan final : public UnscopedTracingSpan<FlatBufferTable> {
 public:
  // Since this manipulates TLS/FLS in RAII fashion, this is intended to be
  // used as a stack local (no heap alloc allowed):
  void* operator new(std::size_t) = delete;
  void* operator new[](std::size_t) = delete;
  // Public constructors allow creating new tracing span.
  template <class... Arg>
  explicit TracingSpan(Arg&&... args)
      : UnscopedTracingSpan<FlatBufferTable>(std::forward<Arg>(args)...) {
    UnscopedTracingSpan<FlatBufferTable>::impl_->Push();
  }
  template <class... Arg>
  explicit TracingSpan(TracingSpanRef parent, Arg&&... args)
      : UnscopedTracingSpan<FlatBufferTable>(parent,
                                             std::forward<Arg>(args)...) {
    UnscopedTracingSpan<FlatBufferTable>::impl_->Push();
  }

  // Destructor closes the span
  ~TracingSpan() override {
    UnscopedTracingSpan<FlatBufferTable>::impl_->Pop();
  }
};

// Writes a trace with the specified args under the topmost span located on
// TLS. If no span exists on TLS, then root span is fetched from the global
// recorder.
template <class FlatBufferTable, class... Arg>
void Trace(Arg&&... args) {
  tracing_internal::AssertIsNotSpan<FlatBufferTable>();
  flatbuffers::DetachedBuffer trace_data =
      tracing_internal::BuildFlatBuffer<FlatBufferTable>(
          std::forward<Arg>(args)...);
  // Now discover what tracing span to log that with:
  tracing_internal::TracingSpanImpl* top =
      tracing_internal::TracingSpanImpl::Top();
  if (top != nullptr) {
    // Fast path: getting top tracing span from TLS/FCB:
    top->TraceImpl(std::move(trace_data), TracingTraits<FlatBufferTable>());
  } else {
    // Slow path, finding root span from global recorder. This
    // involves increasing its reference counter:
    std::shared_ptr<tracing_internal::TracingRecorderImpl> recorder =
        tracing_internal::TracingRecorderImpl::GetCurrent();
    recorder->GetRootSpan()->TraceImpl(std::move(trace_data),
                                       TracingTraits<FlatBufferTable>());
    // now, since we done with using root span, it is safe to release shared_ptr
  }
}

// Writes a trace under the specified span with the given args.
template <class FlatBufferTable, class... Arg>
void Trace(TracingSpanRef span, Arg&&... args) {
  tracing_internal::AssertIsNotSpan<FlatBufferTable>();
  flatbuffers::DetachedBuffer trace_data =
      tracing_internal::BuildFlatBuffer<FlatBufferTable>(
          std::forward<Arg>(args)...);
  span.recorder()->TraceImpl(span.span_id(), std::move(trace_data),
                             TracingTraits<FlatBufferTable>());
}

// Writes an error trace with the specified args under the topmost span located
// on TLS. If no span exists on TLS, then root span is fetched from the global
// recorder.
template <class FlatBufferTable, class... Arg>
ABSL_MUST_USE_RESULT Error TraceError(Arg&&... args) {
  tracing_internal::AssertIsError<FlatBufferTable>();
  Trace<FlatBufferTable>(args...);
  return Error(Error::ConstructorAccess{});
}

}  // namespace fcp

#endif  // FCP_TRACING_TRACING_SPAN_H_
