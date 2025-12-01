/*
 * Copyright 2025 Google LLC
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

#include "fcp/confidentialcompute/composing_tee_executor.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "tensorflow/core/platform/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "fcp/confidentialcompute/constants.h"
#include "fcp/confidentialcompute/test_utils.h"
#include "fcp/testing/parse_text_proto.h"
#include "fcp/testing/testing.h"
#include "federated_language/proto/computation.pb.h"
#include "tensorflow/core/protobuf/error_codes.pb.h"
#include "tensorflow_federated/cc/core/impl/executors/cardinalities.h"
#include "tensorflow_federated/cc/core/impl/executors/composing_executor.h"
#include "tensorflow_federated/cc/core/impl/executors/executor.h"
#include "tensorflow_federated/cc/core/impl/executors/executor_test_base.h"
#include "tensorflow_federated/cc/core/impl/executors/mock_executor.h"
#include "tensorflow_federated/cc/core/impl/executors/value_test_utils.h"
#include "tensorflow_federated/proto/v0/executor.pb.h"

namespace fcp {
namespace confidential_compute {

namespace {

using ::federated_language::FunctionType;
using ::federated_language::Type;
using ::tensorflow_federated::testing::IntrinsicV;
using ::tensorflow_federated::testing::ServerV;
using ::tensorflow_federated::testing::StructV;
using ::tensorflow_federated::testing::TensorV;
using ::testing::HasSubstr;
using ::testing::StrictMock;

class ComposingTeeExecutorTest : public tensorflow_federated::ExecutorTestBase {
 protected:
  void SetUp() override {
    constexpr int kClientsPerChild = 2;
    constexpr int kNumChildExecutors = 4;

    std::vector<tensorflow_federated::ComposingChild> composing_children;
    for (int i = 0; i < kNumChildExecutors; ++i) {
      auto exec =
          std::make_shared<StrictMock<tensorflow_federated::MockExecutor>>();
      auto child_or_status = tensorflow_federated::ComposingChild::Make(
          exec, {{"clients", kClientsPerChild}});
      ASSERT_OK(child_or_status);
      tensorflow_federated::ComposingChild child =
          std::move(child_or_status.value());
      composing_children.push_back(child);
      mock_children_.push_back(std::move(exec));
    }
    mock_server_ =
        std::make_shared<StrictMock<tensorflow_federated::MockExecutor>>();
    test_executor_ =
        CreateComposingTeeExecutor(mock_server_, std::move(composing_children));
  }

  std::shared_ptr<StrictMock<tensorflow_federated::MockExecutor>> mock_server_;
  std::vector<std::shared_ptr<StrictMock<tensorflow_federated::MockExecutor>>>
      mock_children_;
};

TEST_F(ComposingTeeExecutorTest, CreateMaterializeTensor) {
  ExpectCreateMaterialize(TensorV(1.0));
}

TEST_F(ComposingTeeExecutorTest, CreateMaterializeAtServer) {
  ExpectCreateMaterialize(ServerV(TensorV(1.0)));
}

TEST_F(ComposingTeeExecutorTest, CreateMaterializeEmptyStruct) {
  ExpectCreateMaterialize(StructV({}));
}

TEST_F(ComposingTeeExecutorTest, CreateMaterializeFlatStruct) {
  std::vector<tensorflow_federated::v0::Value> elements = {
      TensorV(1.0), TensorV(3.5), TensorV(2.0)};
  ExpectCreateMaterialize(StructV(elements));
}

TEST_F(ComposingTeeExecutorTest, CreateMaterializeDataFileInfo) {
  ExpectCreateMaterialize(CreateFileInfoValue("uri", "key"));
}

TEST_F(ComposingTeeExecutorTest, CreateCallComposedTee) {
  tensorflow_federated::v0::Value accumulate_arg = TensorV("accumulate_arg");
  tensorflow_federated::v0::Value accumulate_fn = TensorV("accumulate_fn");
  tensorflow_federated::v0::Value report_arg = TensorV("report_arg");
  tensorflow_federated::v0::Value report_fn = TensorV("report_fn");
  std::string result_from_child_uri = "result_from_child";
  std::string result_from_server_uri = "result_from_server";

  // Generate type information for the leaf (accumulate) intrinsic.
  Type accumulate_arg_type = PARSE_TEXT_PROTO(R"pb(
    tensor { dtype: DT_INT32 }
  )pb");

  Type accumulate_result_type_pre_aggregates = PARSE_TEXT_PROTO(R"pb(
    tensor { dtype: DT_INT32 }
  )pb");

  Type accumulate_result_type_partial_aggregates = PARSE_TEXT_PROTO(R"pb(
    tensor { dtype: DT_INT64 }
  )pb");

  Type accumulate_result_type;
  *accumulate_result_type.mutable_struct_()->add_element()->mutable_value() =
      accumulate_result_type_pre_aggregates;
  *accumulate_result_type.mutable_struct_()->add_element()->mutable_value() =
      accumulate_result_type_partial_aggregates;

  Type accumulate_fn_type;
  FunctionType* accumulate_fn_type_fn = accumulate_fn_type.mutable_function();
  *accumulate_fn_type_fn->mutable_parameter() = accumulate_arg_type;
  *accumulate_fn_type_fn->mutable_result() = accumulate_result_type;

  FunctionType leaf_intrinsic_fn_type;
  *leaf_intrinsic_fn_type.mutable_parameter()
       ->mutable_struct_()
       ->add_element()
       ->mutable_value() = accumulate_arg_type;
  *leaf_intrinsic_fn_type.mutable_parameter()
       ->mutable_struct_()
       ->add_element()
       ->mutable_value() = accumulate_fn_type;
  *leaf_intrinsic_fn_type.mutable_result() = accumulate_result_type;
  // Generate type information for the root (report) intrinsic.
  Type report_arg_type_combined_partial_aggregates;
  *report_arg_type_combined_partial_aggregates.mutable_struct_()
       ->add_element()
       ->mutable_value() = accumulate_result_type_partial_aggregates;
  *report_arg_type_combined_partial_aggregates.mutable_struct_()
       ->add_element()
       ->mutable_value() = accumulate_result_type_partial_aggregates;
  *report_arg_type_combined_partial_aggregates.mutable_struct_()
       ->add_element()
       ->mutable_value() = accumulate_result_type_partial_aggregates;
  *report_arg_type_combined_partial_aggregates.mutable_struct_()
       ->add_element()
       ->mutable_value() = accumulate_result_type_partial_aggregates;
  Type report_arg_type_remaining = PARSE_TEXT_PROTO(R"pb(
    tensor { dtype: DT_FLOAT }
  )pb");
  Type report_result_type = PARSE_TEXT_PROTO(R"pb(
    tensor { dtype: DT_DOUBLE }
  )pb");
  Type report_fn_type;
  *report_fn_type.mutable_function()
       ->mutable_parameter()
       ->mutable_struct_()
       ->add_element()
       ->mutable_value() = report_arg_type_combined_partial_aggregates;
  *report_fn_type.mutable_function()
       ->mutable_parameter()
       ->mutable_struct_()
       ->add_element()
       ->mutable_value() = report_arg_type_remaining;
  *report_fn_type.mutable_function()->mutable_result() = report_result_type;

  FunctionType root_intrinsic_fn_type;
  Type root_intrinsic_fn_arg_type_parameter_element_0;
  *root_intrinsic_fn_arg_type_parameter_element_0.mutable_struct_()
       ->add_element()
       ->mutable_value() = report_arg_type_combined_partial_aggregates;
  *root_intrinsic_fn_arg_type_parameter_element_0.mutable_struct_()
       ->add_element()
       ->mutable_value() = report_arg_type_remaining;
  *root_intrinsic_fn_type.mutable_parameter()
       ->mutable_struct_()
       ->add_element()
       ->mutable_value() = root_intrinsic_fn_arg_type_parameter_element_0;
  *root_intrinsic_fn_type.mutable_parameter()
       ->mutable_struct_()
       ->add_element()
       ->mutable_value() = report_fn_type;
  *root_intrinsic_fn_type.mutable_result() = report_result_type;

  // Generate type information for the overall composed_tee intrinsic.
  FunctionType intrinsic_fn_type;
  *intrinsic_fn_type.mutable_parameter()
       ->mutable_struct_()
       ->add_element()
       ->mutable_value() = accumulate_arg_type;
  *intrinsic_fn_type.mutable_parameter()
       ->mutable_struct_()
       ->add_element()
       ->mutable_value() = report_arg_type_remaining;
  *intrinsic_fn_type.mutable_parameter()
       ->mutable_struct_()
       ->add_element()
       ->mutable_value() = accumulate_fn_type;
  *intrinsic_fn_type.mutable_parameter()
       ->mutable_struct_()
       ->add_element()
       ->mutable_value() = report_fn_type;
  *intrinsic_fn_type.mutable_result() = report_result_type;

  // Register the expected calls on the child executors.
  for (int child_index = 0; child_index < mock_children_.size();
       child_index++) {
    const std::shared_ptr<StrictMock<tensorflow_federated::MockExecutor>>
        child = mock_children_[child_index];

    tensorflow_federated::ValueId child_accumulate_arg =
        child->ExpectCreateValue(accumulate_arg);
    tensorflow_federated::ValueId child_accumulate_fn =
        child->ExpectCreateValue(accumulate_fn);
    tensorflow_federated::ValueId arg =
        child->ExpectCreateStruct({child_accumulate_arg, child_accumulate_fn});
    tensorflow_federated::ValueId child_agg = child->ExpectCreateValue(
        IntrinsicV(kComposedTeeLeafUri, leaf_intrinsic_fn_type));
    tensorflow_federated::ValueId res = child->ExpectCreateCall(child_agg, arg);
    child->ExpectMaterialize(
        res,
        CreateFileInfoValue(absl::StrCat(result_from_child_uri, child_index),
                            /*key=*/std::nullopt,
                            /*type=*/accumulate_result_type));
  }

  // Register the expected calls on the server executor.
  std::vector<tensorflow_federated::ValueId> server_accumulate_output_args;
  for (int child_index = 0; child_index < mock_children_.size();
       child_index++) {
    tensorflow_federated::ValueId server_accumulate_output =
        mock_server_->ExpectCreateValue(CreateFileInfoValue(
            absl::StrCat(result_from_child_uri, child_index),
            /*key=*/std::nullopt,
            /*type=*/accumulate_result_type));
    server_accumulate_output_args.push_back(mock_server_->ExpectCreateSelection(
        server_accumulate_output, kPartialAggregateOutputIndex));
  }
  tensorflow_federated::ValueId server_accumulate_output_args_combined =
      mock_server_->ExpectCreateStruct(server_accumulate_output_args);
  tensorflow_federated::ValueId server_original_report_arg =
      mock_server_->ExpectCreateValue(report_arg);
  tensorflow_federated::ValueId server_report_arg =
      mock_server_->ExpectCreateStruct(
          {server_accumulate_output_args_combined, server_original_report_arg});

  tensorflow_federated::ValueId server_report_fn =
      mock_server_->ExpectCreateValue(report_fn);
  tensorflow_federated::ValueId arg =
      mock_server_->ExpectCreateStruct({server_report_arg, server_report_fn});

  tensorflow_federated::ValueId server_agg = mock_server_->ExpectCreateValue(
      IntrinsicV(kComposedTeeRootUri, root_intrinsic_fn_type));
  tensorflow_federated::ValueId res =
      mock_server_->ExpectCreateCall(server_agg, arg);
  tensorflow_federated::v0::Value result_from_server =
      CreateFileInfoValue(result_from_server_uri, /*key=*/std::nullopt,
                          /*type=*/report_result_type);
  mock_server_->ExpectMaterialize(res, result_from_server);

  // Create the composed_tee intrinsic call on the test executor and
  // materialize the result.
  auto controller_accumulate_arg_or_status =
      test_executor_->CreateValue(accumulate_arg);
  ASSERT_OK(controller_accumulate_arg_or_status);
  tensorflow_federated::OwnedValueId controller_accumulate_arg =
      std::move(controller_accumulate_arg_or_status.value());
  auto controller_accumulate_fn_or_status =
      test_executor_->CreateValue(accumulate_fn);
  ASSERT_OK(controller_accumulate_fn_or_status);
  tensorflow_federated::OwnedValueId controller_accumulate_fn =
      std::move(controller_accumulate_fn_or_status.value());
  auto controller_report_arg_or_status =
      test_executor_->CreateValue(report_arg);
  ASSERT_OK(controller_report_arg_or_status);
  tensorflow_federated::OwnedValueId controller_report_arg =
      std::move(controller_report_arg_or_status.value());
  auto controller_report_fn_or_status = test_executor_->CreateValue(report_fn);
  ASSERT_OK(controller_report_fn_or_status);
  tensorflow_federated::OwnedValueId controller_report_fn =
      std::move(controller_report_fn_or_status.value());
  auto controller_agg_or_status = test_executor_->CreateValue(
      IntrinsicV(kComposedTeeUri, intrinsic_fn_type));
  ASSERT_OK(controller_agg_or_status);
  tensorflow_federated::OwnedValueId controller_agg =
      std::move(controller_agg_or_status.value());
  auto controller_agg_arg_or_status = test_executor_->CreateStruct(
      {controller_accumulate_arg, controller_report_arg,
       controller_accumulate_fn, controller_report_fn});
  ASSERT_OK(controller_agg_arg_or_status);
  tensorflow_federated::OwnedValueId controller_agg_arg =
      std::move(controller_agg_arg_or_status.value());
  auto controller_res_or_status =
      test_executor_->CreateCall(controller_agg, controller_agg_arg);
  ASSERT_OK(controller_res_or_status);
  tensorflow_federated::OwnedValueId controller_res =
      std::move(controller_res_or_status.value());
  auto controller_res_pb_or_status =
      test_executor_->Materialize(controller_res);
  ASSERT_OK(controller_res_pb_or_status);
  tensorflow_federated::v0::Value controller_res_pb =
      std::move(controller_res_pb_or_status.value());

  // Check that the result is as expected.
  tensorflow_federated::v0::Value expected_result = PARSE_TEXT_PROTO(R"pb(
    struct {
      element {
        value {
          federated {
            type {
              placement { value { uri: "clients" } }
              all_equal: false
              member { tensor { dtype: DT_INT32 } }
            }
            value {
              computation {
                type { tensor { dtype: DT_INT32 } }
                data {
                  content {
                    [type.googleapis.com/fcp.confidentialcompute
                         .FileInfo] { uri: "result_from_child0" key: "0" }
                  }
                }
              }
            }
            value {
              computation {
                type { tensor { dtype: DT_INT32 } }
                data {
                  content {
                    [type.googleapis.com/fcp.confidentialcompute
                         .FileInfo] { uri: "result_from_child1" key: "0" }
                  }
                }
              }
            }
            value {
              computation {
                type { tensor { dtype: DT_INT32 } }
                data {
                  content {
                    [type.googleapis.com/fcp.confidentialcompute
                         .FileInfo] { uri: "result_from_child2" key: "0" }
                  }
                }
              }
            }
            value {
              computation {
                type { tensor { dtype: DT_INT32 } }
                data {
                  content {
                    [type.googleapis.com/fcp.confidentialcompute
                         .FileInfo] { uri: "result_from_child3" key: "0" }
                  }
                }
              }
            }
          }
        }
      }
      element {
        value {
          computation {
            type { tensor { dtype: DT_DOUBLE } }
            data {
              content {
                [type.googleapis.com/fcp.confidentialcompute.FileInfo] {
                  uri: "result_from_server"
                }
              }
            }
          }
        }
      }
    }
  )pb");
  ASSERT_THAT(controller_res_pb, EqualsProto(expected_result));
}

TEST_F(ComposingTeeExecutorTest, CreateCallInvalidIntrinsic) {
  tensorflow_federated::v0::Value arg = TensorV("arg");
  tensorflow_federated::v0::Value intrinsic = IntrinsicV("invalid");

  auto controller_arg_or_status = test_executor_->CreateValue(arg);
  ASSERT_OK(controller_arg_or_status);
  tensorflow_federated::OwnedValueId controller_arg =
      std::move(controller_arg_or_status.value());
  auto controller_intrinsic_or_status = test_executor_->CreateValue(intrinsic);
  ASSERT_OK(controller_intrinsic_or_status);
  tensorflow_federated::OwnedValueId controller_intrinsic =
      std::move(controller_intrinsic_or_status.value());
  auto controller_res_or_status =
      test_executor_->CreateCall(controller_intrinsic, controller_arg);
  ASSERT_OK(controller_res_or_status);
  tensorflow_federated::OwnedValueId controller_res =
      std::move(controller_res_or_status.value());

  EXPECT_THAT(
      test_executor_->Materialize(controller_res),
      tensorflow::testing::StatusIs(absl::StatusCode::kInvalidArgument,
                             HasSubstr("Unsupported intrinsic invalid")));
}

}  // namespace

}  // namespace confidential_compute
}  // namespace fcp
