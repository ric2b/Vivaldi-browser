/*
 * Copyright 2020 Google LLC
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

#include "fcp/tensorflow/tf_session.h"

#include <string>

#include "gtest/gtest.h"
#include "fcp/base/tracing_schema.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/tensorflow/testing/tf_helper.h"
#include "fcp/tensorflow/tracing_schema.h"
#include "fcp/testing/result_matchers.h"
#include "fcp/tracing/test_tracing_recorder.h"
#include "tensorflow/cc/framework/scope.h"
#include "tensorflow/cc/ops/math_ops.h"
#include "tensorflow/cc/ops/standard_ops.h"
#include "tensorflow/core/framework/tensor_testutil.h"
#include "tensorflow/core/protobuf/saver.pb.h"

namespace fcp {

using google::internal::federated::plan::CheckpointOp;
using tensorflow::Tensor;
using tensorflow::TensorShape;
using tensorflow::ops::Add;
using tensorflow::ops::Assign;
using tensorflow::ops::Const;
using tensorflow::ops::Mul;
using tensorflow::ops::Placeholder;
using tensorflow::ops::Restore;
using tensorflow::ops::Save;
using tensorflow::ops::Variable;
using tensorflow::test::AsTensor;
using tensorflow::test::ExpectTensorEqual;
using testing::_;
using testing::Not;

template <typename T>
void CheckOutput(TfSession* sess, const std::string& output_op,
                 Tensor expected) {
  Result<std::unique_ptr<TfSession::NamedTensorMap>> outputs =
      sess->GetOutputs(std::make_unique<std::vector<std::string>>(
          std::initializer_list<std::string>{output_op}));
  EXPECT_THAT(outputs, Not(IsError()));
  ExpectTensorEqual<T>((*outputs.GetValueOrDie())[output_op], expected);
}

TEST(TfSessionTest, InitializeWithEmptyGraph) {
  tensorflow::Scope root = tensorflow::Scope::NewRootScope();
  TestTracingRecorder tracing_recorder;
  TfSession sess("foo/bar", CreateGraph(&root));
  ASSERT_THAT(sess.Ready(), Not(IsError()));
  // Running an empty operation is a no-op.
  EXPECT_THAT(sess.RunOp(""), Not(IsError()));
  // Getting an empty list of outputs is a no-op.
  EXPECT_THAT(sess.GetOutputs(std::make_unique<std::vector<std::string>>()),
              Not(IsError()));
  // There are no ops registered in the GraphDef, so trying to run an op won't
  // work.
  tracing_recorder.ExpectError<ResultExpectStatusError>();
  EXPECT_THAT(sess.RunOp("sum"), IsError());
  // Validate the expected hierarchy of tracing spans. There should be only one
  // RunTfOp span, as we don't want to bother recording a noop if the op is
  // empty.
  EXPECT_THAT(tracing_recorder.root(),
              ElementsAre(AllOf(
                  IsSpan<RunTfOp>(),
                  ElementsAre(IsEvent<ResultExpectStatusError>(
                      static_cast<int>(fcp::OK),
                      static_cast<int>(fcp::INVALID_ARGUMENT), _, _, _)))));
}

TEST(TfSessionTest, InvalidGraphBytes) {
  tensorflow::Scope root = tensorflow::Scope::NewRootScope();
  TestTracingRecorder tracing_recorder;
  tracing_recorder.ExpectError<ResultExpectStatusError>();
  TfSession sess("foo/bar", "garbage");
  ASSERT_THAT(sess.Ready(), IsError());
  EXPECT_THAT(tracing_recorder.root(),
              ElementsAre(IsEvent<ResultExpectStatusError>(
                  static_cast<int>(fcp::OK),
                  static_cast<int>(fcp::INVALID_ARGUMENT), _, _, _)));
}

TEST(TfSessionTest, RunGraphOp) {
  // Construct a TensorFlow graph with all desired operations.
  // This graph just assigns the result of multiplying two constants "a" and "b"
  // to a variable "c", and makes it possible to output "c".
  tensorflow::Scope root = tensorflow::Scope::NewRootScope();
  auto a = Const<int32_t>(root, {{1, 2}, {3, 4}});
  auto b = Const<int32_t>(root, {{2}});
  auto c = Variable(root.WithOpName("c"), {2, 2}, tensorflow::DT_INT32);
  auto assign_c = Assign(root.WithOpName("assign_c"), c, Mul(root, a, b));

  // Run a session using the graph constructed above.
  TestTracingRecorder tracing_recorder;
  TfSession sess("foo/bar", CreateGraph(&root));
  ASSERT_THAT(sess.Ready(), Not(IsError()));

  // Run an operation on the session and validate the result.
  EXPECT_THAT(sess.RunOp("assign_c"), Not(IsError()));
  CheckOutput<int32_t>(&sess, "c",
                       AsTensor<int32_t>({2, 4, 6, 8}, TensorShape({2, 2})));
}

TEST(TfSessionTest, RestoreFromTensor) {
  // Construct a TensorFlow graph with all desired operations.
  tensorflow::Scope root = tensorflow::Scope::NewRootScope();
  auto input = Placeholder(root.WithOpName("i"), tensorflow::DT_INT32);
  auto a = Variable(root.WithOpName("a"), {2, 2}, tensorflow::DT_INT32);
  auto restore = Assign(root.WithOpName("restore_a"), a, input);
  auto double_a = Assign(root.WithOpName("double_a"), a,
                         Mul(root, a, Const<int32_t>(root, {{2}})));

  // Run a session using the graph constructed above.
  TestTracingRecorder tracing_recorder;
  TfSession sess(testing::TempDir(), CreateGraph(&root));
  ASSERT_THAT(sess.Ready(), Not(IsError()));

  CheckpointOp restore_checkpoint_op;
  restore_checkpoint_op.set_before_restore_op("restore_a");
  restore_checkpoint_op.set_after_restore_op("double_a");

  tensorflow::Input::Initializer i({{1, 2}, {3, 4}});
  EXPECT_THAT(sess.RestoreState(restore_checkpoint_op, {{"i", i.tensor}}),
              Not(IsError()));

  CheckOutput<int32_t>(&sess, "a",
                       AsTensor<int32_t>({2, 4, 6, 8}, TensorShape({2, 2})));
}

TEST(TfSessionTest, RestoreFromTensorNoSaverDefAllowed) {
  // Construct a TensorFlow graph with all desired operations.
  tensorflow::Scope root = tensorflow::Scope::NewRootScope();
  auto input = Placeholder(root.WithOpName("i"), tensorflow::DT_INT32);
  auto a = Variable(root, {2, 2}, tensorflow::DT_INT32);
  auto restore = Assign(root.WithOpName("restore_a"), a, input);
  auto double_a = Assign(root.WithOpName("double_a"), a,
                         Mul(root, a, Const<int32_t>(root, {{2}})));

  // Run a session using the graph constructed above.
  TestTracingRecorder tracing_recorder;
  tracing_recorder.ExpectError<InvalidCheckpointOp>();
  TfSession sess(testing::TempDir(), CreateGraph(&root));
  ASSERT_THAT(sess.Ready(), Not(IsError()));

  CheckpointOp restore_checkpoint_op;
  restore_checkpoint_op.set_before_restore_op("restore_a");
  restore_checkpoint_op.mutable_saver_def()->set_restore_op_name("restore");
  restore_checkpoint_op.mutable_saver_def()->set_filename_tensor_name(
      "filename");
  restore_checkpoint_op.set_after_restore_op("double_a");

  tensorflow::Input::Initializer i({{1, 2}, {3, 4}});
  EXPECT_THAT(sess.RestoreState(restore_checkpoint_op, {{"i", i.tensor}}),
              IsError());
}

TEST(TfSessionTest, SaveAndRestoreCheckpointBytes) {
  // Construct a TensorFlow graph with all desired operations.
  tensorflow::Scope root = tensorflow::Scope::NewRootScope();
  auto a = Const<int32_t>(root, {{1, 2}, {3, 4}});
  // Save the current value of constant "a" in a serialized checkpoint.
  auto filename =
      Placeholder(root.WithOpName("filename"), tensorflow::DT_STRING);
  auto save_a = Save(root.WithOpName("save"), filename, {"a"},
                     std::initializer_list<tensorflow::Input>{a});
  // Restore the value saved in the serialized checkpoint to variable "c".
  auto c = Variable(root.WithOpName("c"), {2, 2}, tensorflow::DT_INT32);
  auto restore = Assign(root.WithOpName("restore"), c,
                        Restore(root, filename, "a", tensorflow::DT_INT32));

  // Run a session using the graph constructed above.
  TestTracingRecorder tracing_recorder;
  TfSession sess(testing::TempDir(), CreateGraph(&root));
  ASSERT_THAT(sess.Ready(), Not(IsError()));

  // Save to a checkpoint.
  CheckpointOp save_checkpoint_op;
  save_checkpoint_op.mutable_saver_def()->set_save_tensor_name("save");
  save_checkpoint_op.mutable_saver_def()->set_filename_tensor_name("filename");
  Result<absl::Cord> save_res = sess.SaveState(save_checkpoint_op);
  EXPECT_THAT(save_res, Not(IsError()));

  // Restore from that checkpoint.
  CheckpointOp restore_checkpoint_op;
  restore_checkpoint_op.mutable_saver_def()->set_restore_op_name("restore");
  restore_checkpoint_op.mutable_saver_def()->set_filename_tensor_name(
      "filename");
  EXPECT_THAT(
      sess.RestoreState(restore_checkpoint_op, save_res.GetValueOrDie()),
      Not(IsError()));

  // Verify the value of variable "c" was loaded properly from the checkpoint.
  CheckOutput<int32_t>(&sess, "c",
                       AsTensor<int32_t>({1, 2, 3, 4}, TensorShape({2, 2})));
}

TEST(TfSessionTest, SaveCheckpointBytesSaveOpInTensorFormat) {
  // Construct a TensorFlow graph with all desired operations.
  tensorflow::Scope root = tensorflow::Scope::NewRootScope();
  auto a = Const<int32_t>(root, {{1, 2}, {3, 4}});
  // Save the current value of variable "a" in a serialized checkpoint.
  auto filename =
      Placeholder(root.WithOpName("filename"), tensorflow::DT_STRING);
  auto save_a = Save(root.WithOpName("save"), filename, {"a"},
                     std::initializer_list<tensorflow::Input>{a});

  // Run a session using the graph constructed above.
  TestTracingRecorder tracing_recorder;
  TfSession sess(testing::TempDir(), CreateGraph(&root));
  ASSERT_THAT(sess.Ready(), Not(IsError()));

  // Ensure that attempting to save doesn't return an error even if the save op
  // is provided in tensor format (with a trailing ":0")
  CheckpointOp save_checkpoint_op;
  save_checkpoint_op.mutable_saver_def()->set_save_tensor_name("save:0");
  save_checkpoint_op.mutable_saver_def()->set_filename_tensor_name("filename");
  Result<absl::Cord> save_res = sess.SaveState(save_checkpoint_op);
  EXPECT_THAT(save_res, Not(IsError()));
}

TEST(TfSessionTest, SaveAndRestoreWithBeforeAndAfterOps) {
  // Construct a TensorFlow graph with all desired operations.
  tensorflow::Scope root = tensorflow::Scope::NewRootScope();
  auto a = Variable(root.WithOpName("a"), {2, 2}, tensorflow::DT_INT32);
  auto b = Variable(root, {1, 1}, tensorflow::DT_INT32);
  auto init_a = Assign(root.WithOpName("init_a"), a,
                       Const<int32_t>(root, {{1, 2}, {3, 4}}));
  auto init_b =
      Assign(root.WithOpName("init_b"), b, Const<int32_t>(root, {{2}}));
  auto mul_a = Assign(root.WithOpName("mul_a"), a, Mul(root, a, b));
  auto inc_b = Assign(root.WithOpName("inc_b"), b,
                      Add(root, b, Const<int32_t>(root, {{1}})));
  // Save the current value of variable "a" in a serialized checkpoint.
  auto filename =
      Placeholder(root.WithOpName("filename"), tensorflow::DT_STRING);
  auto save_a = Save(root.WithOpName("save"), filename, {"a"},
                     std::initializer_list<tensorflow::Input>{a});
  // Restore the value saved in the serialized checkpoint to variable "a".
  auto restore = Assign(root.WithOpName("restore"), a,
                        Restore(root, filename, "a", tensorflow::DT_INT32));

  // Run a session using the graph constructed above.
  TestTracingRecorder tracing_recorder;
  TfSession sess(testing::TempDir(), CreateGraph(&root));
  ASSERT_THAT(sess.Ready(), Not(IsError()));
  EXPECT_THAT(sess.RunOp("init_a"), Not(IsError()));
  EXPECT_THAT(sess.RunOp("init_b"), Not(IsError()));

  // Set "a = a * b" and save that value to a checkpoint, then reset "a" to its
  // initial state.
  CheckpointOp save_checkpoint_op;
  save_checkpoint_op.set_before_save_op("mul_a");
  save_checkpoint_op.mutable_saver_def()->set_save_tensor_name("save");
  save_checkpoint_op.mutable_saver_def()->set_filename_tensor_name("filename");
  save_checkpoint_op.set_after_save_op("init_a");
  Result<absl::Cord> save_res = sess.SaveState(save_checkpoint_op);
  EXPECT_THAT(save_res, Not(IsError()));
  // Check that the value of variable "a" has been reset to the initial value by
  // the after_save_op.
  CheckOutput<int32_t>(&sess, "a",
                       AsTensor<int32_t>({1, 2, 3, 4}, TensorShape({2, 2})));

  // Increment "b" to 3 in the before_restore_op, set "a" to the value from the
  // checkpoint, then set "a = a * b".
  CheckpointOp restore_checkpoint_op;
  restore_checkpoint_op.set_before_restore_op("inc_b");
  restore_checkpoint_op.mutable_saver_def()->set_restore_op_name("restore");
  restore_checkpoint_op.mutable_saver_def()->set_filename_tensor_name(
      "filename");
  restore_checkpoint_op.set_after_restore_op("mul_a");
  EXPECT_THAT(
      sess.RestoreState(restore_checkpoint_op, save_res.GetValueOrDie()),
      Not(IsError()));
  // The initial value of "a" should have been multiplied by 2 in the
  // before_save_op and multiplied by 3 in the after_restore_op.
  CheckOutput<int32_t>(&sess, "a",
                       AsTensor<int32_t>({6, 12, 18, 24}, TensorShape({2, 2})));
}

}  // namespace fcp
