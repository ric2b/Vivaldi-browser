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

#include <string>

#include "google/protobuf/any.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "fcp/client/selector_context.pb.h"
#include "fcp/client/task_result_info.pb.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/testing/testing.h"

namespace fcp::client {
namespace {

constexpr absl::string_view kSessionName = "MySession";
constexpr absl::string_view kCollectionUri = "app://my_collection";

using ::google::internal::federated::plan::ExampleSelector;

// Test creating an ExampleIteratorQueryRecorder, but no query was issued. After
// stopping recording, we should still have a valid ExampleIteratorQueries with
// no queries.
TEST(ExampleIteratorQueryRecorderTest, NoQueries) {
  SelectorContext selector_context;
  selector_context.mutable_computation_properties()->set_session_name(
      std::string(kSessionName));
  ExampleIteratorQueryRecorderImpl recorder(selector_context);

  ExampleIteratorQueries expected_queries;
  *expected_queries.mutable_selector_context() = selector_context;
  EXPECT_THAT(recorder.StopRecordingAndGetQueries(),
              EqualsProto(expected_queries));
}

// Test recording a single query with two examples. This query only has
// collection uri, no criteria.
TEST(ExampleIteratorQueryRecorderTest, StartedQueryWithOnlyCollectionUri) {
  SelectorContext selector_context;
  selector_context.mutable_computation_properties()->set_session_name(
      std::string(kSessionName));
  ExampleIteratorQueryRecorderImpl recorder(selector_context);

  ExampleSelector selector;
  selector.set_collection_uri(std::string(kCollectionUri));
  SingleExampleIteratorQueryRecorder* single_query_recorder =
      recorder.RecordQuery(selector);
  single_query_recorder->Increment();
  single_query_recorder->Increment();

  ExampleIteratorQueries expected_queries;
  *expected_queries.mutable_selector_context() = selector_context;
  auto* expected_single_query = expected_queries.mutable_query()->Add();
  expected_single_query->set_example_count(2);
  expected_single_query->set_collection_uri(std::string(kCollectionUri));
  // We will have an empty criteria field.
  *expected_single_query->mutable_criteria() = selector.criteria();
  EXPECT_THAT(recorder.StopRecordingAndGetQueries(),
              EqualsProto(expected_queries));
}

// Test recording a single query with two examples. This query has
// both collection uri and selection criteria.
TEST(ExampleIteratorQueryRecorderTest, StartedQuery) {
  SelectorContext selector_context;
  selector_context.mutable_computation_properties()->set_session_name(
      std::string(kSessionName));
  ExampleIteratorQueryRecorderImpl recorder(selector_context);

  ExampleSelector selector;
  selector.set_collection_uri(std::string(kCollectionUri));
  selector.mutable_criteria()->set_value("arbitrary_bytes");
  SingleExampleIteratorQueryRecorder* single_query_recorder =
      recorder.RecordQuery(selector);
  single_query_recorder->Increment();
  single_query_recorder->Increment();

  ExampleIteratorQueries expected_queries;
  *expected_queries.mutable_selector_context() = selector_context;
  auto* expected_single_query = expected_queries.mutable_query()->Add();
  expected_single_query->set_example_count(2);
  expected_single_query->set_collection_uri(std::string(kCollectionUri));
  // We will have an empty criteria field.
  *expected_single_query->mutable_criteria() = selector.criteria();
  EXPECT_THAT(recorder.StopRecordingAndGetQueries(),
              EqualsProto(expected_queries));
}

// Test recording a single query, but the query is never used (no increment).
TEST(ExampleIteratorQueryRecorderTest, StartedQueryNoExamples) {
  SelectorContext selector_context;
  selector_context.mutable_computation_properties()->set_session_name(
      std::string(kSessionName));
  ExampleIteratorQueryRecorderImpl recorder(selector_context);

  ExampleSelector selector;
  selector.set_collection_uri(std::string(kCollectionUri));
  selector.mutable_criteria()->set_value("arbitrary_bytes");
  recorder.RecordQuery(selector);

  ExampleIteratorQueries expected_queries;
  *expected_queries.mutable_selector_context() = selector_context;
  auto* expected_single_query = expected_queries.mutable_query()->Add();
  expected_single_query->set_example_count(0);
  expected_single_query->set_collection_uri(std::string(kCollectionUri));
  *expected_single_query->mutable_criteria() = selector.criteria();
  EXPECT_THAT(recorder.StopRecordingAndGetQueries(),
              EqualsProto(expected_queries));
}

// Test recording two queries
TEST(ExampleIteratorQueryRecorderTest, StartedTwoQuery) {
  SelectorContext selector_context;
  selector_context.mutable_computation_properties()->set_session_name(
      std::string(kSessionName));
  ExampleIteratorQueryRecorderImpl recorder(selector_context);

  ExampleSelector selector;
  selector.set_collection_uri(std::string(kCollectionUri));
  selector.mutable_criteria()->set_value("arbitrary_bytes");

  ExampleSelector selector_2;
  selector_2.set_collection_uri("another_collection");
  selector_2.mutable_criteria()->set_value("some_other_bytes");
  {
    SingleExampleIteratorQueryRecorder* single_recorder_1 =
        recorder.RecordQuery(selector);
    single_recorder_1->Increment();
    SingleExampleIteratorQueryRecorder* single_recorder_2 =
        recorder.RecordQuery(selector_2);
    single_recorder_2->Increment();

    single_recorder_1->Increment();
    single_recorder_2->Increment();
    single_recorder_2->Increment();
  }

  ExampleIteratorQueries expected_queries;
  *expected_queries.mutable_selector_context() = selector_context;
  auto* first_single_query = expected_queries.mutable_query()->Add();
  first_single_query->set_example_count(2);
  first_single_query->set_collection_uri(std::string(kCollectionUri));
  *first_single_query->mutable_criteria() = selector.criteria();

  auto* second_single_query = expected_queries.mutable_query()->Add();
  second_single_query->set_example_count(3);
  second_single_query->set_collection_uri(selector_2.collection_uri());
  *second_single_query->mutable_criteria() = selector_2.criteria();

  EXPECT_THAT(recorder.StopRecordingAndGetQueries(),
              EqualsProto(expected_queries));
}

// Test recording a query, stop recording, and then start recording again.
// This is not a typical use pattern, but we want to make sure we don't
// accidentally pollute the returned queries with previous states.
TEST(ExampleIteratorQueryRecorderTest, RecordStopThenRecordAgain) {
  SelectorContext selector_context;
  selector_context.mutable_computation_properties()->set_session_name(
      std::string(kSessionName));
  ExampleIteratorQueryRecorderImpl recorder(selector_context);

  ExampleSelector selector;
  selector.set_collection_uri(std::string(kCollectionUri));
  selector.mutable_criteria()->set_value("arbitrary_bytes");
  {
    SingleExampleIteratorQueryRecorder* single_recorder_1 =
        recorder.RecordQuery(selector);
    single_recorder_1->Increment();
    single_recorder_1->Increment();
  }
  ExampleIteratorQueries expected_queries;
  *expected_queries.mutable_selector_context() = selector_context;
  auto* first_single_query = expected_queries.mutable_query()->Add();
  first_single_query->set_example_count(2);
  first_single_query->set_collection_uri(std::string(kCollectionUri));
  *first_single_query->mutable_criteria() = selector.criteria();
  EXPECT_THAT(recorder.StopRecordingAndGetQueries(),
              EqualsProto(expected_queries));

  ExampleSelector selector_2;
  selector_2.set_collection_uri("another_collection");
  selector_2.mutable_criteria()->set_value("some_other_bytes");
  {
    SingleExampleIteratorQueryRecorder* single_recorder_2 =
        recorder.RecordQuery(selector_2);
    single_recorder_2->Increment();
    single_recorder_2->Increment();
    single_recorder_2->Increment();
  }

  ExampleIteratorQueries another_expected_queries;
  *another_expected_queries.mutable_selector_context() = selector_context;
  auto* second_single_query = another_expected_queries.mutable_query()->Add();
  second_single_query->set_example_count(3);
  second_single_query->set_collection_uri(selector_2.collection_uri());
  *second_single_query->mutable_criteria() = selector_2.criteria();
  EXPECT_THAT(recorder.StopRecordingAndGetQueries(),
              EqualsProto(another_expected_queries));
}
}  // anonymous namespace
}  // namespace fcp::client
