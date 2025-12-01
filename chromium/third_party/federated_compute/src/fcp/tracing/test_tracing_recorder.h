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

#ifndef FCP_TRACING_TEST_TRACING_RECORDER_H_
#define FCP_TRACING_TEST_TRACING_RECORDER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "fcp/base/source_location.h"
#include "fcp/tracing/test_tracing_recorder_impl.h"
#include "fcp/tracing/tracing_recorder.h"

namespace fcp {

// Tracing recorder recording all interactions for use in unit tests.
// Automatically installs itself to be a global for its lifetime.
//
// Provides functionality for setting expected errors and failing if either
// an error is seen that was not expected, or one of the expected errors is
// never seen.
//
// Also provides functionality for searching for particular tracing spans or
// events, and verifying expectations about the children of a span or event
// (if desired, one could write a test that verifies the entire expected tree
// of traces.) See fcp/tracing/test/tracing_test.cc for examples.
//
// Upon creation of a tracing span or event, a Record representing the trace is
// added to a centrally owned hashmap (which is protected by a lock, as tracing
// spans and events may be created from multiple threads.) Each record stores
// a reference to its parent.
//
// When verifying the expected traces in a test, the tree structure of the
// traces is reconstructed by searching through the values in the map for
// children of a given parent ID. An ordering of siblings is re-established
// by sorting by the tracing span ID. This strategy is due to the fact that
// children of a single parent may be created from multiple threads (think a
// parent spawning many fibers.)
//
// By not creating the tree structure until it is needed in a test, we avoid
// additional locking on a parent node at trace creation time. This does incur
// a performance penalty when verifying expectations, however- if one is to
// verify the full tree of traces the overhead to reconstruct the tree will be
// O(n^2).
class TestTracingRecorder
    : public TracingRecorder,
      tracing_internal::TestTracingRecorderImpl::TraceListener {
 public:
  explicit TestTracingRecorder(SourceLocation loc = SourceLocation::current());
  ~TestTracingRecorder() override;

  // Allowlists a given error type as expected (non-allowlisted errors will
  // trigger testing assertions.)
  template <typename FlatBufferTable>
  void ExpectError();

  struct Record {
    explicit Record(TracingSpanId parent_id, TracingSpanId id,
                    flatbuffers::DetachedBuffer data)
        : parent_id(parent_id), id(id), data(std::move(data)) {}
    explicit Record(TracingSpanId id, flatbuffers::DetachedBuffer data)
        : parent_id(std::nullopt), id(id), data(std::move(data)) {}
    std::optional<TracingSpanId> parent_id;
    TracingSpanId id;
    flatbuffers::DetachedBuffer data;
  };

  template <typename FlatBufferTable>
  class Span;
  template <typename FlatBufferTable>
  class Event;

  // Allows to dynamically access properties of spans or event, associated
  // with an encapsulated tracing record.
  class SpanOrEvent {
   public:
    explicit SpanOrEvent(TestTracingRecorder* recorder, Record* record);

    // To integrate easily woth googletest/gmock, SpanOrEvent behaves like
    // a collection of children SpanOrEvent objects. Following group of methods
    // mimic STL collection interface by delegating calls to std::vector.
    using value_type = SpanOrEvent;
    using iterator = std::vector<SpanOrEvent>::iterator;
    using const_iterator = std::vector<SpanOrEvent>::const_iterator;
    const_iterator begin() const { return children_.begin(); }
    const_iterator end() const { return children_.end(); }
    const SpanOrEvent& operator[](size_t idx) { return children_.at(idx); }
    bool empty() const { return children_.empty(); }
    size_t size() const { return children_.size(); }

    // Checks if span/event is of certain type:
    template <typename FlatBufferTable>
    bool HasType() const;

    // Returns typed flatbuffer pointer (fails if type mismatch)
    template <typename FlatBufferTable>
    const FlatBufferTable* data() const;

    // Creates text representation:
    std::string TextFormat() const;

    // Find all spans of given type recursively:
    template <typename FlatBufferTable>
    std::vector<Span<FlatBufferTable>> FindAllSpans();

    // Find all events of given type recursively:
    template <typename FlatBufferTable>
    std::vector<Event<FlatBufferTable>> FindAllEvents();

    // Find exactly one span recursively, fails if not found
    template <typename FlatBufferTable>
    Span<FlatBufferTable> FindOnlySpan(
        SourceLocation loc = SourceLocation::current());

    // Find exactly one event recursively, fails if not found
    template <typename FlatBufferTable>
    Event<FlatBufferTable> FindOnlyEvent(
        SourceLocation loc = SourceLocation::current());

    TracingTraitsBase const* traits() const;

   protected:
    Record* record_;

   private:
    std::vector<SpanOrEvent> children_;
    TestTracingRecorder* recorder_;
  };

  // This is used to access root span
  class RootSpan : public SpanOrEvent {
   public:
    explicit RootSpan(TestTracingRecorder* recorder, Record* record)
        : SpanOrEvent(recorder, record) {}
  };

  // Strongly-typed accessor for spans
  template <typename FlatBufferTable>
  class Span : public SpanOrEvent {
   public:
    explicit Span(TestTracingRecorder* recorder, Record* record);

    // Re-declaring SpanOrEvent::data() for convenience, so there's no need
    // to specify type argument when calling it.
    const FlatBufferTable* data() const;
  };

  // Strongly-typed accessor for events
  template <typename FlatBufferTable>
  class Event : public SpanOrEvent {
   public:
    explicit Event(TestTracingRecorder* recorder, Record* record);

    // Re-declaring SpanOrEvent::data() for convenience, so there's no need
    // to specify type argument when calling it.
    const FlatBufferTable* data() const;
  };

  RootSpan root();

  template <typename FlatBufferTable>
  std::vector<Span<FlatBufferTable>> FindAllSpans() {
    return root().FindAllSpans<FlatBufferTable>();
  }

  template <typename FlatBufferTable>
  std::vector<Event<FlatBufferTable>> FindAllEvents() {
    return root().FindAllEvents<FlatBufferTable>();
  }

  template <typename FlatBufferTable>
  Span<FlatBufferTable> FindOnlySpan(
      SourceLocation loc = SourceLocation::current()) {
    return root().FindOnlySpan<FlatBufferTable>(loc);
  }

  template <typename FlatBufferTable>
  Event<FlatBufferTable> FindOnlyEvent(
      SourceLocation loc = SourceLocation::current()) {
    return root().FindOnlyEvent<FlatBufferTable>(loc);
  }

 private:
  void InstallAsGlobal() override;
  void UninstallAsGlobal() override;
  void InstallAsThreadLocal() override;
  void UninstallAsThreadLocal() override;

  // Retrieves all children of the trace represented by the provided record
  // by iterating through a hashmap to identify children that reference this
  // record's ID as their parent.
  //
  // parent is borrowed by this function and must live for the duration of the
  // function.
  //
  // The records in the returned vector are borrowed by the caller and will live
  // until the destruction of this TestTracingRecorder.
  std::vector<Record*> GetChildren(Record* parent);
  // Implements TraceListener interface to react to creation of the root span by
  // creating a record representing the root span, saving it in a map of all
  // traces, and saving a pointer to it.
  void OnRoot(TracingSpanId id, flatbuffers::DetachedBuffer data) override;
  // Implements TraceListener interface which creates a record
  // representing the span or event identified by id, adds it to the map of all
  // traces to make it possible to find for verifying test expectations. Also
  // checks of this is an event that has error severity and if so, checks that
  // this is an expected error as registered by ExpectError().
  void OnTrace(TracingSpanId parent_id, TracingSpanId id,
               flatbuffers::DetachedBuffer data) override;

  SourceLocation loc_;
  Record* root_record_ = nullptr;
  absl::Mutex map_lock_;
  // Global map for storing traces. This map will only grow during the
  // lifetime of this TestTracingRecorder (elements will never be removed or
  // replaced.)
  absl::flat_hash_map<TracingSpanId, std::unique_ptr<Record>> id_to_record_map_
      ABSL_GUARDED_BY(map_lock_);
  std::shared_ptr<tracing_internal::TestTracingRecorderImpl> impl_;

  absl::Mutex expected_errors_lock_;
  // Expectations registered by the test for error events that should be created
  // during test execution.
  absl::flat_hash_set<TracingTag> expected_errors_
      ABSL_GUARDED_BY(expected_errors_lock_);
  // Errors that are expected but have not yet been seen. If any are present on
  // destruction of this object, a precondition check will fail.
  absl::flat_hash_set<TracingTag> unseen_expected_errors_
      ABSL_GUARDED_BY(expected_errors_lock_);
};

template <typename FlatBufferTable>
void TestTracingRecorder::ExpectError() {
  absl::MutexLock locked(&expected_errors_lock_);
  expected_errors_.insert(TracingTraits<FlatBufferTable>::kTag);
  unseen_expected_errors_.insert(TracingTraits<FlatBufferTable>::kTag);
}

template <typename FlatBufferTable>
TestTracingRecorder::Event<FlatBufferTable>::Event(
    TestTracingRecorder* recorder, Record* record)
    : SpanOrEvent(recorder, record) {
  static_assert(!TracingTraits<FlatBufferTable>::kIsSpan,
                "FlatBufferTable must be an event, not a span");
}

template <typename FlatBufferTable>
const FlatBufferTable* TestTracingRecorder::Event<FlatBufferTable>::data()
    const {
  return SpanOrEvent::data<FlatBufferTable>();
}

template <typename FlatBufferTable>
TestTracingRecorder::Span<FlatBufferTable>::Span(TestTracingRecorder* recorder,
                                                 Record* record)
    : SpanOrEvent(recorder, record) {
  static_assert(TracingTraits<FlatBufferTable>::kIsSpan,
                "FlatBufferTable must be a span");
}

template <typename FlatBufferTable>
const FlatBufferTable* TestTracingRecorder::Span<FlatBufferTable>::data()
    const {
  return SpanOrEvent::data<FlatBufferTable>();
}

template <typename FlatBufferTable>
std::vector<TestTracingRecorder::Span<FlatBufferTable>>
TestTracingRecorder::SpanOrEvent::FindAllSpans() {
  static_assert(TracingTraits<FlatBufferTable>::kIsSpan,
                "FlatBufferTable must be a span");
  std::vector<TestTracingRecorder::Span<FlatBufferTable>> result;
  if (HasType<FlatBufferTable>()) {
    result.emplace_back(recorder_, record_);
  }
  for (auto& c : children_) {
    auto child_result = c.FindAllSpans<FlatBufferTable>();
    result.insert(result.end(), child_result.begin(), child_result.end());
  }
  return result;
}

template <typename FlatBufferTable>
std::vector<TestTracingRecorder::Event<FlatBufferTable>>
TestTracingRecorder::SpanOrEvent::FindAllEvents() {
  static_assert(!TracingTraits<FlatBufferTable>::kIsSpan,
                "FlatBufferTable must be an event not a span");
  std::vector<TestTracingRecorder::Event<FlatBufferTable>> result;
  if (HasType<FlatBufferTable>()) {
    result.emplace_back(recorder_, record_);
  }
  for (auto& c : children_) {
    auto child_result = c.FindAllEvents<FlatBufferTable>();
    result.insert(result.end(), child_result.begin(), child_result.end());
  }
  return result;
}

template <typename FlatBufferTable>
TestTracingRecorder::Span<FlatBufferTable>
TestTracingRecorder::SpanOrEvent::FindOnlySpan(SourceLocation loc) {
  auto all_spans = FindAllSpans<FlatBufferTable>();
  EXPECT_THAT(all_spans, testing::SizeIs(1))
      << "Expected exactly one span of type "
      << TracingTraits<FlatBufferTable>().Name() << ". " << std::endl
      << "Source location: " << std::endl
      << loc.file_name() << ":" << loc.line();
  return all_spans[0];
}

template <typename FlatBufferTable>
TestTracingRecorder::Event<FlatBufferTable>
TestTracingRecorder::SpanOrEvent::FindOnlyEvent(SourceLocation loc) {
  auto all_events = FindAllEvents<FlatBufferTable>();
  EXPECT_THAT(all_events, testing::SizeIs(1))
      << "Expected exactly one event of type "
      << TracingTraits<FlatBufferTable>().Name() << ". " << std::endl
      << "Source location: " << std::endl
      << loc.file_name() << ":" << loc.line();
  return all_events[0];
}

template <typename FlatBufferTable>
bool TestTracingRecorder::SpanOrEvent::HasType() const {
  return *TracingTag::FromFlatbuf(record_->data) ==
         TracingTraits<FlatBufferTable>::kTag;
}

template <typename FlatBufferTable>
const FlatBufferTable* TestTracingRecorder::SpanOrEvent::data() const {
  return flatbuffers::GetRoot<FlatBufferTable>(record_->data.data());
}

using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;

inline void PrintTo(const TestTracingRecorder::SpanOrEvent& value,
                    std::ostream* os) {
  *os << value.TextFormat();
}

// This wraps std::tuple with method .get<I>() as a member. This
// allows to compose a tuple matcher from multiple testing::Property matchers,
// combined with testing::AllOf.
template <typename Tuple>
class TupleWrapper {
 public:
  explicit TupleWrapper(Tuple const& tuple) : tuple_(tuple) {}
  template <std::size_t I>
  const typename std::tuple_element<I, Tuple>::type& get() const {
    return std::get<I>(tuple_);
  }

 private:
  const Tuple tuple_;
};

// Universal matcher for spans and events, checks for type and content.
template <typename FlatBufferTable>
class SpanOrEventTypeMatcher
    : public MatcherInterface<const TestTracingRecorder::SpanOrEvent&> {
 public:
  using TupleType = typename TracingTraits<FlatBufferTable>::TupleType;
  using ContentMatcher = testing::Matcher<TupleWrapper<TupleType>>;

  explicit SpanOrEventTypeMatcher(absl::string_view kind,
                                  ContentMatcher content_matcher)
      : kind_(kind), content_matcher_(content_matcher) {}

  bool MatchAndExplain(const TestTracingRecorder::SpanOrEvent& value,
                       MatchResultListener* listener) const override {
    *listener << " { " << value.TextFormat() << " } ";
    bool result = value.HasType<FlatBufferTable>();
    if (result) {
      auto content =
          TupleWrapper<TupleType>(TracingTraits<FlatBufferTable>::MakeTuple(
              value.data<FlatBufferTable>()));
      result = content_matcher_.MatchAndExplain(content, listener);
    }
    return result;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "Expecting " << kind_ << " of type "
        << TracingTraits<FlatBufferTable>().Name() << " with fields ";
    content_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "Expecting NOT " << kind_ << " of type "
        << TracingTraits<FlatBufferTable>().Name() << " with fields ";
    content_matcher_.DescribeNegationTo(os);
  }

 private:
  std::string kind_;
  ContentMatcher content_matcher_;
};

template <typename Tuple, typename... M, std::size_t... I>
auto MatchTupleWrapper(M... m, std::index_sequence<I...>) {
  return testing::AllOf(
      testing::Property(&TupleWrapper<Tuple>::template get<I>, m)...);
}

template <typename Tuple, typename... M>
testing::Matcher<TupleWrapper<Tuple>> MatchTupleElements(M... m) {
  return testing::MatcherCast<TupleWrapper<Tuple>>(
      MatchTupleWrapper<Tuple, M...>(m...,
                                     std::make_index_sequence<sizeof...(M)>{}));
}

template <typename FlatBufferTable, typename... M>
Matcher<const TestTracingRecorder::SpanOrEvent&> IsSpan(M... field_matchers) {
  static_assert(TracingTraits<FlatBufferTable>::kIsSpan,
                "FlatBufferTable must be a span");
  if constexpr (sizeof...(M) != 0) {
    constexpr size_t number_of_fields = std::tuple_size<
        typename TracingTraits<FlatBufferTable>::TupleType>::value;
    static_assert(
        sizeof...(M) == number_of_fields,
        "Matchers must be provided for every field in FlatBufferTable");
    return MakeMatcher(new SpanOrEventTypeMatcher<FlatBufferTable>(
        "span",
        MatchTupleElements<typename TracingTraits<FlatBufferTable>::TupleType>(
            field_matchers...)));
  } else {
    // When no field matchers provided it should match anything
    return MakeMatcher(
        new SpanOrEventTypeMatcher<FlatBufferTable>("span", testing::_));
  }
}

template <typename FlatBufferTable, typename... M>
Matcher<const TestTracingRecorder::SpanOrEvent&> IsEvent(M... field_matchers) {
  static_assert(!TracingTraits<FlatBufferTable>::kIsSpan,
                "FlatBufferTable must not be a span");
  if constexpr (sizeof...(M) != 0) {
    return MakeMatcher(new SpanOrEventTypeMatcher<FlatBufferTable>(
        "event",
        MatchTupleElements<typename TracingTraits<FlatBufferTable>::TupleType>(
            field_matchers...)));
  } else {
    // When no field matchers provided it should match anything
    return MakeMatcher(
        new SpanOrEventTypeMatcher<FlatBufferTable>("event", testing::_));
  }
}

}  // namespace fcp

#endif  // FCP_TRACING_TEST_TRACING_RECORDER_H_
