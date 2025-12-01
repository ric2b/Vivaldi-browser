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

#include "fcp/confidentialcompute/tee_executor_value.h"

#include <memory>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/testing/parse_text_proto.h"
#include "fcp/testing/testing.h"
#include "federated_language/proto/computation.pb.h"
#include "tensorflow_federated/cc/core/impl/executors/value_test_utils.h"
#include "tensorflow_federated/proto/v0/executor.pb.h"

namespace fcp {
namespace confidential_compute {

namespace {

using ::tensorflow_federated::testing::StructV;
using ::tensorflow_federated::testing::TensorV;
using ::testing::SizeIs;

MATCHER_P(IsOkAndHolds, m, "") {
  return testing::ExplainMatchResult(IsOk(), arg, result_listener) &&
         testing::ExplainMatchResult(m, arg.value(), result_listener);
}

TEST(TeeExecutorValueTest, CreateTensorValue) {
  tensorflow_federated::v0::Value value_pb = TensorV(1.0);
  ExecutorValue value(value_pb);

  EXPECT_THAT(value.value(), IsOkAndHolds(EqualsProto(value_pb)));
  EXPECT_THAT(value.resolve_to_value(), IsOkAndHolds(EqualsProto(value_pb)));
}

TEST(TeeExecutorValueTest, CreateDataFileInfoValue) {
  tensorflow_federated::v0::Value value_pb = PARSE_TEXT_PROTO(R"pb(
    computation {
      data {
        content {
          [type.googleapis.com/
           fcp.confidentialcompute.FileInfo] { uri: "uri" key: "key" }
        }
      }
    }
  )pb");
  ExecutorValue value(value_pb);

  EXPECT_THAT(value.value(), IsOkAndHolds(EqualsProto(value_pb)));
  EXPECT_THAT(value.resolve_to_value(), IsOkAndHolds(EqualsProto(value_pb)));
}

TEST(TeeExecutorValueTest, CreateStruct) {
  tensorflow_federated::v0::Value value_pb_1 = TensorV(1.0);
  tensorflow_federated::v0::Value value_pb_2 = TensorV(2.0);
  ExecutorValue value_1(value_pb_1);
  ExecutorValue value_2(value_pb_2);
  std::vector<ExecutorValue> values = {value_1, value_2};
  ExecutorValue struct_value(
      std::make_shared<std::vector<ExecutorValue>>(std::move(values)));

  auto retrieval_or_status = struct_value.struct_elements();
  ASSERT_OK(retrieval_or_status);
  const std::shared_ptr<std::vector<ExecutorValue>> retrieval =
      retrieval_or_status.value();
  ASSERT_THAT(*retrieval, SizeIs(2));
  EXPECT_THAT(*retrieval->at(0).value(), EqualsProto(value_pb_1));
  EXPECT_THAT(*retrieval->at(1).value(), EqualsProto(value_pb_2));

  EXPECT_THAT(struct_value.resolve_to_value(),
              IsOkAndHolds(EqualsProto(StructV({value_pb_1, value_pb_2}))));
}

TEST(TeeExecutorValueTest, CreateEmptyStruct) {
  std::vector<ExecutorValue> values = {};
  ExecutorValue struct_value(
      std::make_shared<std::vector<ExecutorValue>>(std::move(values)));

  auto retrieval_or_status = struct_value.struct_elements();
  ASSERT_OK(retrieval_or_status);
  const std::shared_ptr<std::vector<ExecutorValue>> retrieval =
      retrieval_or_status.value();

  EXPECT_THAT(struct_value.resolve_to_value(),
              IsOkAndHolds(EqualsProto(StructV({}))));
}

TEST(TeeExecutorValueTest, CreateSelectionOnStruct) {
  tensorflow_federated::v0::Value value_pb_1 = TensorV(1.0);
  tensorflow_federated::v0::Value value_pb_2 = TensorV(2.0);
  ExecutorValue value_1(value_pb_1);
  ExecutorValue value_2(value_pb_2);
  std::vector<ExecutorValue> values = {value_1, value_2};
  ExecutorValue struct_value(
      std::make_shared<std::vector<ExecutorValue>>(std::move(values)));
  ExecutorValue selection_value(std::make_shared<ExecutorValue>(struct_value),
                                1);

  auto retrieval_or_status = selection_value.selection();
  ASSERT_OK(retrieval_or_status);
  const ExecutorValue::Selection retrieval = retrieval_or_status.value();
  EXPECT_THAT(*retrieval.source->struct_elements().value(), SizeIs(2));
  EXPECT_EQ(retrieval.index, 1);

  EXPECT_THAT(selection_value.resolve_to_value(),
              IsOkAndHolds(EqualsProto(value_pb_2)));
}

TEST(TeeExecutorValueTest, CreateSelectionOnDataFileInfo) {
  tensorflow_federated::v0::Value value_pb = PARSE_TEXT_PROTO(R"pb(
    computation {
      type {
        struct {
          element { value { tensor { dtype: DT_INT32 } } }
          element { value { tensor { dtype: DT_DOUBLE } } }
        }
      }
      data {
        content {
          [type.googleapis.com/
           fcp.confidentialcompute.FileInfo] { uri: "uri" key: "key" }
        }
      }
    }
  )pb");
  ExecutorValue value(value_pb);
  ExecutorValue selection_value(std::make_shared<ExecutorValue>(value), 0);

  auto retrieval_or_status = selection_value.selection();
  ASSERT_OK(retrieval_or_status);
  const ExecutorValue::Selection retrieval = retrieval_or_status.value();
  EXPECT_THAT(*retrieval.source->value(), EqualsProto(value_pb));
  EXPECT_EQ(retrieval.index, 0);

  tensorflow_federated::v0::Value expected_resolved_value_pb =
      PARSE_TEXT_PROTO(R"pb(
        computation {
          type { tensor { dtype: DT_INT32 } }
          data {
            content {
              [type.googleapis.com/
               fcp.confidentialcompute.FileInfo] { uri: "uri" key: "key/0" }
            }
          }
        }
      )pb");
  EXPECT_THAT(selection_value.resolve_to_value(),
              IsOkAndHolds(EqualsProto(expected_resolved_value_pb)));
}

}  // namespace

}  // namespace confidential_compute
}  // namespace fcp
