/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fcp/client/example_iterator_query_recorder.h"

#include <memory>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "absl/synchronization/mutex.h"
#include "fcp/client/task_result_info.pb.h"

namespace fcp::client {

void SingleExampleIteratorQueryRecorderImpl::Increment() {
  absl::MutexLock lock(&mutex_);
  example_count_++;
}

SingleExampleIteratorQuery
SingleExampleIteratorQueryRecorderImpl::FinishRecordingAndGet() {
  absl::MutexLock lock(&mutex_);
  SingleExampleIteratorQuery query;
  query.set_collection_uri(selector_.collection_uri());
  *query.mutable_criteria() = selector_.criteria();
  query.set_example_count(example_count_);
  // Reset the example count in case the recorder is reused after
  // FinishRecordingAndGet is called.
  example_count_ = 0;
  return query;
}

SingleExampleIteratorQueryRecorder*
ExampleIteratorQueryRecorderImpl::RecordQuery(
    const google::internal::federated::plan::ExampleSelector& selector) {
  absl::MutexLock lock(&mutex_);
  auto recorder =
      std::make_unique<SingleExampleIteratorQueryRecorderImpl>(selector);
  SingleExampleIteratorQueryRecorder* raw_pointer = recorder.get();
  recorders_.push_back(std::move(recorder));
  return raw_pointer;
}

ExampleIteratorQueries
ExampleIteratorQueryRecorderImpl::StopRecordingAndGetQueries() {
  absl::MutexLock lock(&mutex_);
  ExampleIteratorQueries queries;
  *queries.mutable_selector_context() = selector_context_;
  auto* single_example_iterator_query_list = queries.mutable_query();
  for (const auto& recorder : recorders_) {
    *single_example_iterator_query_list->Add() =
        recorder->FinishRecordingAndGet();
  }
  recorders_.clear();
  return queries;
}

}  // namespace fcp::client
