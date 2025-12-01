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

#include "fcp/confidentialcompute/tee_executor.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "tensorflow/core/platform/status_matchers.h"
#include "absl/status/statusor.h"
#include "fcp/confidentialcompute/constants.h"
#include "fcp/confidentialcompute/mock_lambda_runner.h"
#include "fcp/confidentialcompute/test_utils.h"
#include "fcp/testing/testing.h"
#include "tensorflow/core/protobuf/error_codes.pb.h"
#include "tensorflow_federated/cc/core/impl/executors/executor.h"
#include "tensorflow_federated/cc/core/impl/executors/executor_test_base.h"
#include "tensorflow_federated/cc/core/impl/executors/value_test_utils.h"
#include "tensorflow_federated/proto/v0/executor.pb.h"

namespace fcp::confidential_compute {

namespace {

using ::fcp::confidential_compute::CreateFileInfoValue;
using ::fcp::confidential_compute::kComposedTeeLeafUri;
using ::tensorflow_federated::OwnedValueId;
using ::tensorflow_federated::testing::IntrinsicV;
using ::tensorflow_federated::testing::TensorV;
using ::testing::HasSubstr;
using ::testing::Optional;
using ::testing::Return;

MATCHER_P(IsOkAndHolds, m, "") {
  return testing::ExplainMatchResult(IsOk(), arg, result_listener) &&
         testing::ExplainMatchResult(m, arg.value(), result_listener);
}

constexpr int kNumClients = 3;

class TeeExecutorTest : public tensorflow_federated::ExecutorTestBase {
 public:
  TeeExecutorTest() {
    test_executor_ = CreateTeeExecutor(lambda_runner_, kNumClients);
  }

 protected:
  std::shared_ptr<MockLambdaRunner> lambda_runner_ =
      std::make_shared<::testing::StrictMock<MockLambdaRunner>>();

  template <typename T>
  T CheckOkAndGetValue(absl::StatusOr<T>&& status_or) {
    CHECK_OK(status_or.status());
    return std::move(status_or.value());
  }
};

TEST_F(TeeExecutorTest, CreateCallComposedTeeComponent) {
  tensorflow_federated::v0::Value accumulate_arg = TensorV("accumulate_arg");
  tensorflow_federated::v0::Value accumulate_fn = TensorV("accumulate_fn");
  tensorflow_federated::v0::Value result =
      CreateFileInfoValue("result from lambda runner");

  EXPECT_CALL(*lambda_runner_,
              ExecuteComp(EqualsProto(accumulate_fn),
                          Optional(EqualsProto(accumulate_arg)), kNumClients))
      .WillOnce(Return(result));
  OwnedValueId controller_accumulate_arg =
      CheckOkAndGetValue(test_executor_->CreateValue(accumulate_arg));
  OwnedValueId controller_accumulate_fn =
      CheckOkAndGetValue(test_executor_->CreateValue(accumulate_fn));
  OwnedValueId controller_agg = CheckOkAndGetValue(
      test_executor_->CreateValue(IntrinsicV(kComposedTeeLeafUri)));
  OwnedValueId controller_agg_arg =
      CheckOkAndGetValue(test_executor_->CreateStruct(
          {controller_accumulate_arg, controller_accumulate_fn}));
  OwnedValueId controller_res = CheckOkAndGetValue(
      test_executor_->CreateCall(controller_agg, controller_agg_arg));

  EXPECT_THAT(test_executor_->Materialize(controller_res),
              IsOkAndHolds(EqualsProto(result)));
}

TEST_F(TeeExecutorTest, CreateCallInvalidIntrinsic) {
  tensorflow_federated::v0::Value arg = TensorV("accumulate_arg");
  OwnedValueId controller_arg =
      CheckOkAndGetValue(test_executor_->CreateValue(arg));
  OwnedValueId controller_intrinsic = CheckOkAndGetValue(
      test_executor_->CreateValue(IntrinsicV("invalid_uri")));
  OwnedValueId controller_res = CheckOkAndGetValue(
      test_executor_->CreateCall(controller_intrinsic, controller_arg));

  EXPECT_THAT(
      test_executor_->Materialize(controller_res),
      tensorflow::testing::StatusIs(absl::StatusCode::kInvalidArgument,
                             HasSubstr("Unsupported intrinsic invalid_uri")));
}

TEST_F(TeeExecutorTest, CreateCallInvalidNumArgs) {
  tensorflow_federated::v0::Value accumulate_arg = TensorV("accumulate_arg");
  tensorflow_federated::v0::Value accumulate_fn = TensorV("accumulate_fn");
  tensorflow_federated::v0::Value extra_arg = TensorV("extra_arg");
  OwnedValueId controller_accumulate_arg =
      CheckOkAndGetValue(test_executor_->CreateValue(accumulate_arg));
  OwnedValueId controller_accumulate_fn =
      CheckOkAndGetValue(test_executor_->CreateValue(accumulate_fn));
  OwnedValueId controller_extra_arg =
      CheckOkAndGetValue(test_executor_->CreateValue(extra_arg));
  OwnedValueId controller_agg = CheckOkAndGetValue(
      test_executor_->CreateValue(IntrinsicV(kComposedTeeLeafUri)));
  OwnedValueId controller_agg_arg =
      CheckOkAndGetValue(test_executor_->CreateStruct(
          {controller_accumulate_arg, controller_accumulate_fn,
           controller_extra_arg}));
  OwnedValueId controller_res = CheckOkAndGetValue(
      test_executor_->CreateCall(controller_agg, controller_agg_arg));

  EXPECT_THAT(test_executor_->Materialize(controller_res),
              tensorflow::testing::StatusIs(absl::StatusCode::kInvalidArgument,
                                     HasSubstr("Expected argument of size 2")));
}

TEST_F(TeeExecutorTest, CreateCallInvalidLambdaRunnerResult) {
  tensorflow_federated::v0::Value accumulate_arg = TensorV("accumulate_arg");
  tensorflow_federated::v0::Value accumulate_fn = TensorV("accumulate_fn");
  tensorflow_federated::v0::Value result = TensorV("invalid_result");

  EXPECT_CALL(*lambda_runner_,
              ExecuteComp(EqualsProto(accumulate_fn),
                          Optional(EqualsProto(accumulate_arg)), kNumClients))
      .WillOnce(Return(result));
  OwnedValueId controller_accumulate_arg =
      CheckOkAndGetValue(test_executor_->CreateValue(accumulate_arg));
  OwnedValueId controller_accumulate_fn =
      CheckOkAndGetValue(test_executor_->CreateValue(accumulate_fn));
  OwnedValueId controller_agg = CheckOkAndGetValue(
      test_executor_->CreateValue(IntrinsicV(kComposedTeeLeafUri)));
  OwnedValueId controller_agg_arg =
      CheckOkAndGetValue(test_executor_->CreateStruct(
          {controller_accumulate_arg, controller_accumulate_fn}));
  OwnedValueId controller_res = CheckOkAndGetValue(
      test_executor_->CreateCall(controller_agg, controller_agg_arg));

  EXPECT_THAT(
      test_executor_->Materialize(controller_res),
      tensorflow::testing::StatusIs(
          absl::StatusCode::kInternal,
          HasSubstr(
              "Expected lambda runner to return a data computation value")));
}

}  // namespace

}  // namespace fcp::confidential_compute
