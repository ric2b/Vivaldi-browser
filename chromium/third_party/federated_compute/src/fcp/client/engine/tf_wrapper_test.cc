/*
 * Copyright 2021 Google LLC
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
#include "fcp/client/engine/tf_wrapper.h"

#include "google/protobuf/any.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "fcp/base/monitoring.h"
#include "fcp/testing/testing.h"
#include "tensorflow/core/protobuf/config.pb.h"

namespace fcp {
namespace client {
namespace engine {
namespace {

using ::google::protobuf::Any;
using ::tensorflow::ConfigProto;

TEST(TfWrapperInitializeConfigProtoTest, InvalidConfigProtoWrongTypeUrl) {
  // Create an Any with a valid value but invalid type URL.
  ConfigProto config_proto;
  config_proto.mutable_graph_options()->set_timeline_step(123);
  Any packed_config_proto;
  packed_config_proto.PackFrom(config_proto);
  packed_config_proto.set_type_url("invalid");

  absl::StatusOr<ConfigProto> result =
      TensorFlowWrapper::InitializeConfigProto(packed_config_proto);

  EXPECT_THAT(result, IsCode(INVALID_ARGUMENT));
}

TEST(TfWrapperInitializeConfigProtoTest, InvalidConfigProtoEmptyTypeUrl) {
  // Create an Any with a valid value but empty type URL.
  ConfigProto config_proto;
  config_proto.mutable_graph_options()->set_timeline_step(123);
  Any packed_config_proto;
  packed_config_proto.PackFrom(config_proto);
  packed_config_proto.clear_type_url();

  absl::StatusOr<ConfigProto> result =
      TensorFlowWrapper::InitializeConfigProto(packed_config_proto);

  EXPECT_THAT(result, IsCode(INVALID_ARGUMENT));
}

TEST(TfWrapperInitializeConfigProtoTest, InvalidConfigProtoValue) {
  // Set the correct type URL, but an unparseable value.
  Any packed_config_proto;
  packed_config_proto.PackFrom(ConfigProto());
  packed_config_proto.set_value("nonparseable");

  absl::StatusOr<ConfigProto> result =
      TensorFlowWrapper::InitializeConfigProto(packed_config_proto);

  EXPECT_THAT(result, IsCode(INVALID_ARGUMENT));
}

TEST(TfWrapperInitializeConfigProtoTest, ValidNonEmptyConfigProtoValue) {
  // Create an Any containing a valid, non-empty ConfigProto.
  ConfigProto config_proto;
  config_proto.mutable_graph_options()->set_timeline_step(123);
  Any packed_config_proto;
  packed_config_proto.PackFrom(config_proto);

  absl::StatusOr<ConfigProto> result =
      TensorFlowWrapper::InitializeConfigProto(packed_config_proto);

  // A non-empty ConfigProto was provided, so it should be used as-is.
  ASSERT_OK(result);
  EXPECT_THAT(*result, EqualsProto(config_proto));
}

TEST(TfWrapperInitializeConfigProtoTest, ValidEmptyConfigProtoValue) {
  // Create an Any containing an empty ConfigProto.
  Any packed_config_proto;
  packed_config_proto.PackFrom(ConfigProto());

  absl::StatusOr<ConfigProto> result =
      TensorFlowWrapper::InitializeConfigProto(packed_config_proto);

  // No external ConfigProto was provided, so the hardcoded defaults should be
  // used.
  ConfigProto expected_config_proto;
  expected_config_proto.mutable_graph_options()->set_place_pruned_graph(true);
  expected_config_proto.mutable_experimental()
      ->set_disable_output_partition_graphs(true);
  expected_config_proto.mutable_experimental()->set_optimize_for_static_graph(
      true);

  ASSERT_OK(result);
  EXPECT_THAT(*result, EqualsProto(expected_config_proto));
}

TEST(TfWrapperInitializeConfigProtoTest, ValidEmptyPackedConfigProtoValue) {
  // Create an empty Any.
  Any packed_config_proto;

  absl::StatusOr<ConfigProto> result =
      TensorFlowWrapper::InitializeConfigProto(packed_config_proto);

  // No external ConfigProto was provided, so the hardcoded defaults should be
  // used.
  ConfigProto expected_config_proto;
  expected_config_proto.mutable_graph_options()->set_place_pruned_graph(true);
  expected_config_proto.mutable_experimental()
      ->set_disable_output_partition_graphs(true);
  expected_config_proto.mutable_experimental()->set_optimize_for_static_graph(
      true);

  ASSERT_OK(result);
  EXPECT_THAT(*result, EqualsProto(expected_config_proto));
}

}  // namespace
}  // namespace engine
}  // namespace client
}  // namespace fcp
