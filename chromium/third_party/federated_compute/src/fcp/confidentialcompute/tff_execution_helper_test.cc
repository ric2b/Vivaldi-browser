// Copyright 2024 Google LLC
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

#include "fcp/confidentialcompute/tff_execution_helper.h"

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "fcp/protos/confidentialcompute/file_info.pb.h"
#include "fcp/testing/testing.h"
#include "federated_language/proto/array.pb.h"
#include "federated_language/proto/computation.pb.h"
#include "tensorflow_federated/cc/core/impl/executors/executor.h"
#include "tensorflow_federated/cc/core/impl/executors/mock_executor.h"
#include "tensorflow_federated/proto/v0/executor.pb.h"

namespace fcp {
namespace confidential_compute {
namespace {

using ::fcp::EqualsProto;
using ::fcp::confidentialcompute::FileInfo;
using ::tensorflow_federated::v0::Value;
using ::testing::HasSubstr;
using ::testing::Return;

TEST(TffExecutionHelperTest, ExtractStructValueOrdinalKey) {
  auto expected_value = tensorflow_federated::v0::Value();
  expected_value.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(20);

  auto unexpected_value = tensorflow_federated::v0::Value();
  unexpected_value.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(-1);

  auto value = tensorflow_federated::v0::Value();
  auto element_1 = value.mutable_struct_()->add_element();
  *element_1->mutable_value() = unexpected_value;
  auto element_2 = value.mutable_struct_()->add_element();
  *element_2->mutable_value() = expected_value;
  auto element_3 = value.mutable_struct_()->add_element();
  *element_3->mutable_value() = unexpected_value;

  auto result = ExtractStructValue(value, "1");
  EXPECT_OK(result.status());
  EXPECT_THAT(result.value(), EqualsProto(expected_value));
}

TEST(TffExecutionHelperTest, ExtractStructValueOrdinalKeyOutOfRange) {
  auto unexpected_value = tensorflow_federated::v0::Value();
  unexpected_value.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(-1);

  auto value = tensorflow_federated::v0::Value();
  auto element_1 = value.mutable_struct_()->add_element();
  *element_1->mutable_value() = unexpected_value;
  auto element_2 = value.mutable_struct_()->add_element();
  *element_2->mutable_value() = unexpected_value;

  auto result = ExtractStructValue(value, "2");
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(result.status().message(), testing::HasSubstr("out of range"));
}

TEST(TffExecutionHelperTest, ExtractStructValueStringKey) {
  auto expected_value = tensorflow_federated::v0::Value();
  expected_value.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(20);

  auto unexpected_value = tensorflow_federated::v0::Value();
  unexpected_value.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(-1);

  auto value = tensorflow_federated::v0::Value();
  auto element_foo = value.mutable_struct_()->add_element();
  element_foo->set_name("foo");
  *element_foo->mutable_value() = unexpected_value;
  auto element_bar = value.mutable_struct_()->add_element();
  element_bar->set_name("bar");
  *element_bar->mutable_value() = expected_value;
  auto element_baz = value.mutable_struct_()->add_element();
  element_baz->set_name("baz");
  *element_baz->mutable_value() = unexpected_value;

  auto result = ExtractStructValue(value, "bar");
  EXPECT_OK(result.status());
  EXPECT_THAT(result.value(), EqualsProto(expected_value));
}

TEST(TffExecutionHelperTest, ExtractStructValueInvalidKeyExtraDelimiter) {
  auto unexpected_value = tensorflow_federated::v0::Value();
  unexpected_value.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(-1);

  auto value = tensorflow_federated::v0::Value();
  auto element_foo = value.mutable_struct_()->add_element();
  element_foo->set_name("foo");
  *element_foo->mutable_value() = unexpected_value;
  auto element_bar = value.mutable_struct_()->add_element();
  element_bar->set_name("bar");
  *element_bar->mutable_value() = unexpected_value;

  auto result = ExtractStructValue(value, "bar/");
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kInvalidArgument));
}

TEST(TffExecutionHelperTest, ExtractStructValueInvalidStringKey) {
  auto unexpected_value = tensorflow_federated::v0::Value();
  unexpected_value.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(-1);

  auto value = tensorflow_federated::v0::Value();
  auto element_foo = value.mutable_struct_()->add_element();
  element_foo->set_name("foo");
  *element_foo->mutable_value() = unexpected_value;
  auto element_bar = value.mutable_struct_()->add_element();
  element_bar->set_name("bar");
  *element_bar->mutable_value() = unexpected_value;

  auto result = ExtractStructValue(value, "surprise");
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(result.status().message(),
              testing::HasSubstr("surprise not found"));
}

TEST(TffExecutionHelperTest, ExtractStructValueNestedKey) {
  auto expected_value = tensorflow_federated::v0::Value();
  expected_value.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(20);

  auto unexpected_value = tensorflow_federated::v0::Value();
  unexpected_value.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(-1);

  auto value = tensorflow_federated::v0::Value();

  auto element_foo = value.mutable_struct_()->add_element();
  element_foo->set_name("foo");
  *element_foo->mutable_value() = unexpected_value;

  auto element_bar = value.mutable_struct_()->add_element();
  element_bar->set_name("bar");
  auto element_bar_mango =
      element_bar->mutable_value()->mutable_struct_()->add_element();
  element_bar_mango->set_name("mango");
  *element_bar_mango->mutable_value() = unexpected_value;
  auto element_bar_apple =
      element_bar->mutable_value()->mutable_struct_()->add_element();
  element_bar_apple->set_name("apple");
  auto element_bar_apple_0 =
      element_bar_apple->mutable_value()->mutable_struct_()->add_element();
  *element_bar_apple_0->mutable_value() = unexpected_value;
  auto element_bar_apple_1 =
      element_bar_apple->mutable_value()->mutable_struct_()->add_element();
  auto element_bar_apple_1_0 =
      element_bar_apple_1->mutable_value()->mutable_struct_()->add_element();
  *element_bar_apple_1_0->mutable_value() = expected_value;

  auto element_baz = value.mutable_struct_()->add_element();
  element_baz->set_name("baz");
  *element_baz->mutable_value() = unexpected_value;

  auto result = ExtractStructValue(value, "bar/apple/1/0");
  EXPECT_OK(result.status());
  EXPECT_THAT(result.value(), EqualsProto(expected_value));
}

TEST(TffExecutionHelperTest, ExtractStructValueNestedInvalidKey) {
  auto unexpected_value = tensorflow_federated::v0::Value();
  unexpected_value.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(-1);

  auto value = tensorflow_federated::v0::Value();

  auto element_foo = value.mutable_struct_()->add_element();
  element_foo->set_name("foo");
  *element_foo->mutable_value() = unexpected_value;

  auto element_bar = value.mutable_struct_()->add_element();
  element_bar->set_name("bar");
  auto element_bar_0 =
      element_bar->mutable_value()->mutable_struct_()->add_element();
  *element_bar_0->mutable_value() = unexpected_value;

  auto result = ExtractStructValue(value, "bar/1");
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(result.status().message(), testing::HasSubstr("out of range"));
}

TEST(TffExecutionHelperTest, ExtractStructValueFederated) {
  auto expected_value = tensorflow_federated::v0::Value();
  expected_value.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(20);

  auto unexpected_value = tensorflow_federated::v0::Value();
  unexpected_value.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(-1);

  federated_language::FederatedType federated_type;
  federated_type.mutable_placement()->mutable_value()->set_uri("server");
  federated_type.set_all_equal(true);

  auto state_value = tensorflow_federated::v0::Value();
  *state_value.mutable_federated()->mutable_type() = federated_type;
  *state_value.mutable_federated()->add_value() = unexpected_value;

  auto metrics_value = tensorflow_federated::v0::Value();
  *metrics_value.mutable_federated()->mutable_type() = federated_type;
  auto metrics_struct =
      metrics_value.mutable_federated()->add_value()->mutable_struct_();
  auto metrics_foo = metrics_struct->add_element();
  metrics_foo->set_name("foo");
  *metrics_foo->mutable_value() = unexpected_value;
  auto metrics_bar = metrics_struct->add_element();
  metrics_bar->set_name("bar");
  *metrics_bar->mutable_value() = expected_value;

  auto state_and_metrics_value = tensorflow_federated::v0::Value();
  auto element = state_and_metrics_value.mutable_struct_()->add_element();
  element->set_name("state");
  *element->mutable_value() = state_value;
  element = state_and_metrics_value.mutable_struct_()->add_element();
  element->set_name("metrics");
  *element->mutable_value() = metrics_value;

  auto result = ExtractStructValue(state_and_metrics_value, "metrics/bar");
  EXPECT_OK(result.status());
  EXPECT_THAT(result.value(), EqualsProto(expected_value));
}

class MockDataFetcher {
 public:
  MOCK_METHOD(absl::StatusOr<tensorflow_federated::v0::Value>, FetchData,
              (const std::string uri), (const));
  MOCK_METHOD(absl::StatusOr<tensorflow_federated::v0::Value>, FetchClientData,
              (const std::string uri, const std::string key), (const));
};

TEST(TffExecutionHelperTest, ReplaceDatasComputationWithData) {
  MockDataFetcher mock_data_fetcher;
  auto expected_value = tensorflow_federated::v0::Value();
  expected_value.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(20);
  EXPECT_CALL(mock_data_fetcher, FetchData("foo"))
      .WillOnce(Return(expected_value));

  auto data_value = tensorflow_federated::v0::Value();
  FileInfo file_info;
  file_info.set_uri("foo");
  data_value.mutable_computation()->mutable_data()->mutable_content()->PackFrom(
      file_info);

  absl::StatusOr<ReplaceDatasResult> result =
      ReplaceDatas(data_value, [&mock_data_fetcher](const std::string uri) {
        return mock_data_fetcher.FetchData(uri);
      });
  EXPECT_OK(result.status());
  EXPECT_THAT(result.value().replaced_value, EqualsProto(expected_value));
}

TEST(TffExecutionHelperTest, ReplaceDatasComputationWithoutData) {
  MockDataFetcher mock_data_fetcher;

  auto expected_value = tensorflow_federated::v0::Value();
  expected_value.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(20);

  // The output should match the input if the original Computation proto was
  // not a Data proto.
  auto result =
      ReplaceDatas(expected_value, [&mock_data_fetcher](const std::string uri) {
        return mock_data_fetcher.FetchData(uri);
      });
  EXPECT_OK(result.status());
  EXPECT_THAT(result.value().replaced_value, EqualsProto(expected_value));
}

TEST(TffExecutionHelperTest, ReplaceDatasFederatedWithClientUploads) {
  MockDataFetcher mock_data_fetcher;
  auto expected_value_1 = tensorflow_federated::v0::Value();
  expected_value_1.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(20);
  auto expected_value_2 = tensorflow_federated::v0::Value();
  expected_value_2.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(40);
  EXPECT_CALL(mock_data_fetcher, FetchClientData("client1", "key"))
      .WillOnce(Return(expected_value_1));
  EXPECT_CALL(mock_data_fetcher, FetchClientData("client2", "key"))
      .WillOnce(Return(expected_value_2));

  auto expected_value = tensorflow_federated::v0::Value();
  expected_value.mutable_federated()
      ->mutable_type()
      ->mutable_placement()
      ->mutable_value()
      ->set_uri("clients");
  expected_value.mutable_federated()->mutable_type()->set_all_equal(false);
  *expected_value.mutable_federated()->add_value() = expected_value_1;
  *expected_value.mutable_federated()->add_value() = expected_value_2;

  FileInfo file_info_1;
  file_info_1.set_uri("client1");
  file_info_1.set_key("key");
  file_info_1.set_client_upload(true);
  FileInfo file_info_2;
  file_info_2.set_uri("client2");
  file_info_2.set_key("key");
  file_info_2.set_client_upload(true);

  auto federated_value = tensorflow_federated::v0::Value();
  federated_value.mutable_federated()
      ->mutable_type()
      ->mutable_placement()
      ->mutable_value()
      ->set_uri("clients");
  federated_value.mutable_federated()->mutable_type()->set_all_equal(false);
  federated_value.mutable_federated()
      ->add_value()
      ->mutable_computation()
      ->mutable_data()
      ->mutable_content()
      ->PackFrom(file_info_1);
  federated_value.mutable_federated()
      ->add_value()
      ->mutable_computation()
      ->mutable_data()
      ->mutable_content()
      ->PackFrom(file_info_2);

  auto result = ReplaceDatas(
      federated_value,
      [&mock_data_fetcher](const std::string uri) {
        return mock_data_fetcher.FetchData(uri);
      },
      [&mock_data_fetcher](const std::string uri, const std::string key) {
        return mock_data_fetcher.FetchClientData(uri, key);
      });
  EXPECT_OK(result.status());
  EXPECT_THAT(result.value().replaced_value, EqualsProto(expected_value));
}

TEST(TffExecutionHelperTest, ReplaceDatasWithClientUploadsNoClientFetchFn) {
  MockDataFetcher mock_data_fetcher;
  auto expected_value_1 = tensorflow_federated::v0::Value();
  expected_value_1.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(20);
  auto expected_value_2 = tensorflow_federated::v0::Value();
  expected_value_2.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(40);

  auto expected_value = tensorflow_federated::v0::Value();
  expected_value.mutable_federated()
      ->mutable_type()
      ->mutable_placement()
      ->mutable_value()
      ->set_uri("clients");
  expected_value.mutable_federated()->mutable_type()->set_all_equal(false);
  *expected_value.mutable_federated()->add_value() = expected_value_1;
  *expected_value.mutable_federated()->add_value() = expected_value_2;

  FileInfo file_info_1;
  file_info_1.set_uri("client1");
  file_info_1.set_key("key");
  file_info_1.set_client_upload(true);
  FileInfo file_info_2;
  file_info_2.set_uri("client2");
  file_info_2.set_key("key");
  file_info_2.set_client_upload(true);

  auto federated_value = tensorflow_federated::v0::Value();
  federated_value.mutable_federated()
      ->mutable_type()
      ->mutable_placement()
      ->mutable_value()
      ->set_uri("clients");
  federated_value.mutable_federated()->mutable_type()->set_all_equal(false);
  federated_value.mutable_federated()
      ->add_value()
      ->mutable_computation()
      ->mutable_data()
      ->mutable_content()
      ->PackFrom(file_info_1);
  federated_value.mutable_federated()
      ->add_value()
      ->mutable_computation()
      ->mutable_data()
      ->mutable_content()
      ->PackFrom(file_info_2);

  auto result = ReplaceDatas(federated_value,
                             [&mock_data_fetcher](const std::string uri) {
                               return mock_data_fetcher.FetchData(uri);
                             });
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(result.status().message(),
              HasSubstr("No function provided to fetch client data"));
}

TEST(TffExecutionHelperTest, ReplaceDatasFederatedWithoutClientUploads) {
  MockDataFetcher mock_data_fetcher;
  auto expected_value = tensorflow_federated::v0::Value();
  expected_value.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(20);
  EXPECT_CALL(mock_data_fetcher, FetchData("wrapped_data"))
      .WillOnce(Return(expected_value));

  FileInfo file_info;
  file_info.set_uri("wrapped_data");

  auto federated_value = tensorflow_federated::v0::Value();
  federated_value.mutable_federated()
      ->mutable_type()
      ->mutable_placement()
      ->mutable_value()
      ->set_uri("clients");
  federated_value.mutable_federated()->mutable_type()->set_all_equal(false);
  federated_value.mutable_federated()
      ->add_value()
      ->mutable_computation()
      ->mutable_data()
      ->mutable_content()
      ->PackFrom(file_info);

  auto result = ReplaceDatas(federated_value,
                             [&mock_data_fetcher](const std::string uri) {
                               return mock_data_fetcher.FetchData(uri);
                             });
  EXPECT_OK(result.status());
  EXPECT_THAT(result.value().replaced_value, EqualsProto(expected_value));
}

TEST(TffExecutionHelperTest, ReplaceDatasFederatedInvalidLength) {
  MockDataFetcher mock_data_fetcher;
  auto client_data = tensorflow_federated::v0::Value();
  client_data.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(-1);
  EXPECT_CALL(mock_data_fetcher, FetchData("wrapped_data_1"))
      .WillOnce(Return(client_data));
  EXPECT_CALL(mock_data_fetcher, FetchData("wrapped_data_2"))
      .WillOnce(Return(client_data));

  FileInfo file_info_1;
  file_info_1.set_uri("wrapped_data_1");
  FileInfo file_info_2;
  file_info_2.set_uri("wrapped_data_2");

  auto federated_value = tensorflow_federated::v0::Value();
  federated_value.mutable_federated()
      ->mutable_type()
      ->mutable_placement()
      ->mutable_value()
      ->set_uri("clients");
  federated_value.mutable_federated()->mutable_type()->set_all_equal(false);
  federated_value.mutable_federated()
      ->add_value()
      ->mutable_computation()
      ->mutable_data()
      ->mutable_content()
      ->PackFrom(file_info_1);
  federated_value.mutable_federated()
      ->add_value()
      ->mutable_computation()
      ->mutable_data()
      ->mutable_content()
      ->PackFrom(file_info_2);

  auto result = ReplaceDatas(federated_value,
                             [&mock_data_fetcher](const std::string uri) {
                               return mock_data_fetcher.FetchData(uri);
                             });
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(result.status().message(),
              HasSubstr("should have exactly one inner value"));
}

TEST(TffExecutionHelperTest, ReplaceDatasStruct) {
  MockDataFetcher mock_data_fetcher;
  auto expected_value_1 = tensorflow_federated::v0::Value();
  expected_value_1.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(20);
  auto expected_value_2 = tensorflow_federated::v0::Value();
  expected_value_2.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(40);
  auto expected_value_3 = tensorflow_federated::v0::Value();
  expected_value_3.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(60);
  EXPECT_CALL(mock_data_fetcher, FetchData("val1"))
      .WillOnce(Return(expected_value_1));
  EXPECT_CALL(mock_data_fetcher, FetchData("val2"))
      .WillOnce(Return(expected_value_2));
  EXPECT_CALL(mock_data_fetcher, FetchData("val3"))
      .WillOnce(Return(expected_value_3));

  // Create an expected value corresponding to [val1, ["bar":val2, "foo":val3]].
  auto expected_value = tensorflow_federated::v0::Value();
  *expected_value.mutable_struct_()->add_element()->mutable_value() =
      expected_value_1;
  auto inner_struct = expected_value.mutable_struct_()
                          ->add_element()
                          ->mutable_value()
                          ->mutable_struct_();
  auto inner_struct_bar = inner_struct->add_element();
  inner_struct_bar->set_name("bar");
  *inner_struct_bar->mutable_value() = expected_value_2;
  auto inner_struct_foo = inner_struct->add_element();
  inner_struct_foo->set_name("foo");
  *inner_struct_foo->mutable_value() = expected_value_3;

  FileInfo file_info_1;
  file_info_1.set_uri("val1");
  FileInfo file_info_2;
  file_info_2.set_uri("val2");
  FileInfo file_info_3;
  file_info_3.set_uri("val3");

  // Create an input value with the same structure as the expected value
  // constructed above, except that the value protos are replaced with data
  // value protos.
  auto input_value = tensorflow_federated::v0::Value();
  input_value.mutable_struct_()
      ->add_element()
      ->mutable_value()
      ->mutable_computation()
      ->mutable_data()
      ->mutable_content()
      ->PackFrom(file_info_1);
  auto input_inner_struct = input_value.mutable_struct_()
                                ->add_element()
                                ->mutable_value()
                                ->mutable_struct_();
  auto input_inner_struct_bar = input_inner_struct->add_element();
  input_inner_struct_bar->set_name("bar");
  input_inner_struct_bar->mutable_value()
      ->mutable_computation()
      ->mutable_data()
      ->mutable_content()
      ->PackFrom(file_info_2);
  auto input_inner_struct_foo = input_inner_struct->add_element();
  input_inner_struct_foo->set_name("foo");
  input_inner_struct_foo->mutable_value()
      ->mutable_computation()
      ->mutable_data()
      ->mutable_content()
      ->PackFrom(file_info_3);

  auto result =
      ReplaceDatas(input_value, [&mock_data_fetcher](const std::string uri) {
        return mock_data_fetcher.FetchData(uri);
      });
  EXPECT_OK(result.status());
  EXPECT_THAT(result.value().replaced_value, EqualsProto(expected_value));
}

TEST(TffExecutionHelperTest, ReplaceDatasWithKeys) {
  MockDataFetcher mock_data_fetcher;
  auto previous_round = tensorflow_federated::v0::Value();

  auto previous_round_state = previous_round.mutable_struct_()->add_element();
  previous_round_state->set_name("state");
  auto expected_value_state = tensorflow_federated::v0::Value();
  expected_value_state.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(20);
  *previous_round_state->mutable_value() = expected_value_state;

  auto previous_round_metrics = previous_round.mutable_struct_()->add_element();
  previous_round_metrics->set_name("metrics");
  auto expected_value_metrics_0 = tensorflow_federated::v0::Value();
  expected_value_metrics_0.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(40);
  *previous_round_metrics->mutable_value()
       ->mutable_struct_()
       ->add_element()
       ->mutable_value() = expected_value_metrics_0;
  auto expected_value_metrics_1 = tensorflow_federated::v0::Value();
  expected_value_metrics_1.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(60);
  *previous_round_metrics->mutable_value()
       ->mutable_struct_()
       ->add_element()
       ->mutable_value() = expected_value_metrics_1;

  EXPECT_CALL(mock_data_fetcher, FetchData("previous_round"))
      .WillRepeatedly(Return(previous_round));

  // Create an expected value corresponding to ["state":20, "metrics":[40, 60]].
  auto expected_value = tensorflow_federated::v0::Value();
  auto inner_state = expected_value.mutable_struct_()->add_element();
  inner_state->set_name("state");
  *inner_state->mutable_value() = expected_value_state;
  auto inner_metrics = expected_value.mutable_struct_()->add_element();
  inner_metrics->set_name("metrics");
  *inner_metrics->mutable_value()
       ->mutable_struct_()
       ->add_element()
       ->mutable_value() = expected_value_metrics_0;
  *inner_metrics->mutable_value()
       ->mutable_struct_()
       ->add_element()
       ->mutable_value() = expected_value_metrics_1;

  FileInfo state_file_info;
  state_file_info.set_uri("previous_round");
  state_file_info.set_key("state");
  FileInfo metrics_0_file_info;
  metrics_0_file_info.set_uri("previous_round");
  metrics_0_file_info.set_key("metrics/0");
  FileInfo metrics_1_file_info;
  metrics_1_file_info.set_uri("previous_round");
  metrics_1_file_info.set_key("metrics/1");

  // Create an input value with the same structure as the expected value
  // constructed above, except that the value protos are replaced with data
  // value protos.
  auto input_value = tensorflow_federated::v0::Value();
  auto input_inner_state = input_value.mutable_struct_()->add_element();
  input_inner_state->set_name("state");
  input_inner_state->mutable_value()
      ->mutable_computation()
      ->mutable_data()
      ->mutable_content()
      ->PackFrom(state_file_info);
  auto input_inner_metrics = input_value.mutable_struct_()->add_element();
  input_inner_metrics->set_name("metrics");
  input_inner_metrics->mutable_value()
      ->mutable_struct_()
      ->add_element()
      ->mutable_value()
      ->mutable_computation()
      ->mutable_data()
      ->mutable_content()
      ->PackFrom(metrics_0_file_info);
  input_inner_metrics->mutable_value()
      ->mutable_struct_()
      ->add_element()
      ->mutable_value()
      ->mutable_computation()
      ->mutable_data()
      ->mutable_content()
      ->PackFrom(metrics_1_file_info);

  auto result =
      ReplaceDatas(input_value, [&mock_data_fetcher](const std::string uri) {
        return mock_data_fetcher.FetchData(uri);
      });
  EXPECT_OK(result.status());
  EXPECT_THAT(result.value().replaced_value, EqualsProto(expected_value));
}

TEST(TffExecutionHelperTest, EmbedUnsupportedValueType) {
  auto mock_executor = std::make_shared<tensorflow_federated::MockExecutor>();
  tensorflow_federated::v0::Value value;
  value.mutable_sequence();
  EXPECT_THAT(Embed(value, mock_executor),
              IsCode(absl::StatusCode::kInvalidArgument));
}

TEST(TffExecutionHelperTest, EmbedComputation) {
  auto mock_executor = std::make_shared<tensorflow_federated::MockExecutor>();
  tensorflow_federated::v0::Value value;
  value.mutable_computation();
  tensorflow_federated::ValueId value_id =
      mock_executor->ExpectCreateValue(value);
  auto result = Embed(value, mock_executor);
  EXPECT_OK(result.status());
  EXPECT_EQ(result.value()->ref(), value_id);
}

TEST(TffExecutionHelperTest, EmbedFederated) {
  auto mock_executor = std::make_shared<tensorflow_federated::MockExecutor>();
  tensorflow_federated::v0::Value value;
  value.mutable_federated();
  tensorflow_federated::ValueId value_id =
      mock_executor->ExpectCreateValue(value);
  auto result = Embed(value, mock_executor);
  EXPECT_OK(result.status());
  EXPECT_EQ(result.value()->ref(), value_id);
}

TEST(TffExecutionHelperTest, EmbedArray) {
  auto mock_executor = std::make_shared<tensorflow_federated::MockExecutor>();
  tensorflow_federated::v0::Value value;
  value.mutable_array();
  tensorflow_federated::ValueId value_id =
      mock_executor->ExpectCreateValue(value);
  auto result = Embed(value, mock_executor);
  EXPECT_OK(result.status());
  EXPECT_EQ(result.value()->ref(), value_id);
}

TEST(TffExecutionHelperTest, EmbedStruct) {
  auto mock_executor = std::make_shared<tensorflow_federated::MockExecutor>();
  auto expected_value_1 = tensorflow_federated::v0::Value();
  expected_value_1.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(20);
  auto expected_value_2 = tensorflow_federated::v0::Value();
  expected_value_2.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(40);
  auto expected_value_3 = tensorflow_federated::v0::Value();
  expected_value_3.mutable_computation()
      ->mutable_literal()
      ->mutable_value()
      ->mutable_int32_list()
      ->add_value(60);

  // Create an expected value corresponding to [val1, ["bar":val2, "foo":val3]].
  auto expected_value = tensorflow_federated::v0::Value();
  *expected_value.mutable_struct_()->add_element()->mutable_value() =
      expected_value_1;
  auto inner_struct = expected_value.mutable_struct_()
                          ->add_element()
                          ->mutable_value()
                          ->mutable_struct_();
  auto inner_struct_bar = inner_struct->add_element();
  inner_struct_bar->set_name("bar");
  *inner_struct_bar->mutable_value() = expected_value_2;
  auto inner_struct_foo = inner_struct->add_element();
  inner_struct_foo->set_name("foo");
  *inner_struct_foo->mutable_value() = expected_value_3;

  tensorflow_federated::ValueId value_id_1 =
      mock_executor->ExpectCreateValue(expected_value_1);
  tensorflow_federated::ValueId value_id_2 =
      mock_executor->ExpectCreateValue(expected_value_2);
  tensorflow_federated::ValueId value_id_3 =
      mock_executor->ExpectCreateValue(expected_value_3);
  tensorflow_federated::ValueId inner_struct_value_id =
      mock_executor->ExpectCreateStruct({value_id_2, value_id_3});
  tensorflow_federated::ValueId struct_value_id =
      mock_executor->ExpectCreateStruct({value_id_1, inner_struct_value_id});
  auto result = Embed(expected_value, mock_executor);
  EXPECT_OK(result.status());
  EXPECT_EQ(result.value()->ref(), struct_value_id);
}

}  // namespace
}  // namespace confidential_compute
}  // namespace fcp
