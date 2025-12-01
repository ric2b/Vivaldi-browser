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

#include "fcp/tracing/test_tracing_recorder.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "fcp/tracing/test_tracing_recorder_impl.h"
#include "fcp/tracing/tracing_severity.h"
#include "fcp/tracing/tracing_tag.h"
#include "fcp/tracing/tracing_traits.h"

namespace fcp {

TestTracingRecorder::TestTracingRecorder(SourceLocation loc)
    : loc_(loc),
      impl_(std::shared_ptr<tracing_internal::TestTracingRecorderImpl>(
          new tracing_internal::TestTracingRecorderImpl(this))) {
  InstallAsGlobal();
}

TestTracingRecorder::~TestTracingRecorder() {
  UninstallAsGlobal();
  std::vector<std::string> unseen_error_names;
  for (TracingTag tag : unseen_expected_errors_) {
    unseen_error_names.push_back(TracingTraitsBase::Lookup(tag)->Name());
  }
  EXPECT_THAT(unseen_error_names, testing::IsEmpty())
      << "Errors marked as expected with TestTracingRecorder::ExpectEvent<E>() "
      << "weren't traced via TestTracingRecorder with source location: "
      << std::endl
      << loc_.file_name() << ":" << loc_.line() << std::endl;
}

TracingTraitsBase const* TraitsForRecord(TestTracingRecorder::Record* record) {
  return TracingTraitsBase::Lookup(*TracingTag::FromFlatbuf(record->data));
}

std::string Format(TracingTraitsBase const* traits,
                   TestTracingRecorder::Record* record) {
  return absl::StrCat(traits->Name(), " ", traits->TextFormat(record->data));
}

void TestTracingRecorder::OnRoot(TracingSpanId id,
                                 flatbuffers::DetachedBuffer data) {
  auto unique_record = std::make_unique<Record>(id, std::move(data));
  root_record_ = unique_record.get();
  {
    absl::MutexLock locked(&map_lock_);
    FCP_CHECK(id_to_record_map_.empty());
    id_to_record_map_[id] = std::move(unique_record);
  }
}

void TestTracingRecorder::OnTrace(TracingSpanId parent_id, TracingSpanId id,
                                  flatbuffers::DetachedBuffer data) {
  auto unique_record = std::make_unique<Record>(parent_id, id, std::move(data));
  // Entries in id_to_record_map_ will never be removed or modified. Due to this
  // invariant we can be sure that a pointer to the record will not be
  // invalidated by a concurrent modification of the map causing destruction of
  // the record.
  Record* record = unique_record.get();
  {
    absl::MutexLock locked(&map_lock_);
    id_to_record_map_[id] = std::move(unique_record);
  }
  TracingTraitsBase const* traits = TraitsForRecord(record);
  if (traits->Severity() == TracingSeverity::kError) {
    absl::MutexLock locked(&expected_errors_lock_);
    // We're interested here in errors only:
    TracingTag error_tag = *TracingTag::FromFlatbuf(record->data);
    EXPECT_TRUE(expected_errors_.contains(error_tag))
        << "Unexpected error " << Format(traits, record)
        << " is traced via TestTracingRecorder with source location: "
        << std::endl
        << loc_.file_name() << ":" << loc_.line() << std::endl
        << "Use TracingRecorder::ExpectError<" << traits->Name()
        << ">() in the beginning of the unit "
        << "test to allowlist this error as expected" << std::endl;
    unseen_expected_errors_.erase(error_tag);
  }
}

TestTracingRecorder::SpanOrEvent::SpanOrEvent(TestTracingRecorder* recorder,
                                              Record* record)
    : record_(record), recorder_(recorder) {
  std::vector<Record*> children = recorder_->GetChildren(record_);

  children_.reserve(children.size());
  for (Record* c : children) {
    children_.emplace_back(recorder_, c);
  }
}

std::string TestTracingRecorder::SpanOrEvent::TextFormat() const {
  return Format(traits(), record_);
}

TracingTraitsBase const* TestTracingRecorder::SpanOrEvent::traits() const {
  return TraitsForRecord(record_);
}

void TestTracingRecorder::InstallAsGlobal() { impl_->InstallAsGlobal(); }

void TestTracingRecorder::UninstallAsGlobal() { impl_->UninstallAsGlobal(); }

void TestTracingRecorder::InstallAsThreadLocal() {
  impl_->InstallAsThreadLocal();
}

void TestTracingRecorder::UninstallAsThreadLocal() {
  impl_->UninstallAsThreadLocal();
}

struct RecordComparison {
  bool operator()(TestTracingRecorder::Record* r1,
                  TestTracingRecorder::Record* r2) const {
    return r1->id < r2->id;
  }
};

std::vector<TestTracingRecorder::Record*> TestTracingRecorder::GetChildren(
    Record* parent) {
  std::vector<Record*> children;
  {
    absl::MutexLock locked(&map_lock_);
    // Search through the entire hashmap for children of this parent.
    // Note that this is O(n) in terms of the total number of traces, rather
    // than in terms of the number of children.
    for (const auto& [id, record_unique_ptr] : id_to_record_map_) {
      Record* record = record_unique_ptr.get();
      if (record->parent_id.has_value() &&
          record->parent_id.value() == parent->id) {
        children.push_back(record);
      }
    }
  }
  // Sort in order of lowest to highest ID, which should be in order of creation
  // time.
  std::sort(children.begin(), children.end(), RecordComparison());
  return children;
}

TestTracingRecorder::RootSpan TestTracingRecorder::root() {
  return RootSpan(this, root_record_);
}

}  // namespace fcp
