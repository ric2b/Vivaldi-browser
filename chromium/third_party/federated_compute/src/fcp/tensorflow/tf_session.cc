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

#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "fcp/base/meta.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/platform.h"
#include "fcp/base/process_unique_id.h"
#include "fcp/base/result.h"
#include "fcp/tensorflow/tracing_schema_generated.h"
#include "fcp/tracing/tracing_span.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/protobuf/saver.pb.h"
#include "tensorflow/core/public/session.h"
#include "tensorflow/core/public/session_options.h"

namespace fcp {

#define TF_STATUS_EXPECT_OK(tf_status) Result(tf_status).Then(ExpectOk())

using CheckpointOp = google::internal::federated::plan::CheckpointOp;

TfSession::TfSession(const std::filesystem::path& tmp_dir,
                     const absl::Cord& graph)
    : tmp_dir_(StripTrailingPathSeparator(tmp_dir.c_str())),
      session_(tensorflow::NewSession(tensorflow::SessionOptions{})) {
  // Parse GraphDef.
  tensorflow::GraphDef graph_def;
  // TODO(team): Replace with ParseFromCord (check if it is available).
  std::string graph_str;
  absl::CopyCordToString(graph, &graph_str);
  if (!graph_def.ParseFromString(graph_str)) {
    session_status_ = FCP_STATUS(INVALID_ARGUMENT)
                      << "Could not parse GraphDef.";
    return;
  }
  session_status_ = session_->Create(graph_def);
}

TfSession::TfSession(const std::filesystem::path& tmp_dir,
                     absl::string_view graph)
    : TfSession(tmp_dir, absl::Cord(graph)) {}

Result<Unit> TfSession::Ready() {
  return Result(session_status_).Then(ExpectOk());
}

Result<Unit> TfSession::RunOp(absl::string_view op) {
  FCP_TRY(Ready());
  if (op.empty()) {
    return Unit{};
  }
  TracingSpan<RunTfOp> span(op);
  std::vector<std::string> target_node_names;
  target_node_names.emplace_back(op);
  FCP_TRY(TF_STATUS_EXPECT_OK(session_->Run(
      /*inputs=*/{},
      /*output_tensor_names=*/{}, target_node_names,
      /*outputs=*/nullptr)));
  return Unit{};
}

Result<Unit> TfSession::RunOp(const NamedTensorList& inputs,
                              absl::string_view op) {
  FCP_TRY(Ready());
  if (op.empty()) {
    return Unit{};
  }
  std::vector<std::string> target_node_names;
  target_node_names.emplace_back(op);
  FCP_TRY(TF_STATUS_EXPECT_OK(session_->Run(inputs,
                                            /*output_tensor_names=*/{},
                                            target_node_names,
                                            /*outputs=*/nullptr)));
  return Unit{};
}

Result<std::unique_ptr<TfSession::NamedTensorMap>> TfSession::GetOutputs(
    std::unique_ptr<std::vector<std::string>> output_names) {
  FCP_TRY(Ready());
  auto outputs = std::make_unique<TfSession::NamedTensorMap>();
  if (output_names->empty()) {
    return std::move(outputs);
  }
  std::vector<tensorflow::Tensor> output_list;
  FCP_TRY(TF_STATUS_EXPECT_OK(session_->Run(
      /*inputs=*/{}, *output_names,
      /*target_tensor_names=*/{}, &output_list)));
  FCP_CHECK(output_names->size() == output_list.size());
  for (int i = 0; i < output_names->size(); i++) {
    outputs->emplace(std::move((*output_names)[i]), std::move(output_list[i]));
  }
  return std::move(outputs);
}

void DeleteTmpFile(const std::string& tmp_file_name) {
  if (std::remove(tmp_file_name.c_str()) > 0) {
    Trace<TmpFileNotDeleted>(tmp_file_name);
  }
}

Result<absl::Cord> TfSession::SaveState(const CheckpointOp& op) {
  FCP_TRY(Ready());
  TracingSpan<SaveToCheckpoint> span(
      op.before_save_op(),
      op.has_saver_def() ? op.saver_def().save_tensor_name() : "",
      op.after_save_op());
  FCP_TRY(RunOp(op.before_save_op()));
  Result<absl::Cord> res = absl::Cord("");
  if (op.has_saver_def()) {
    const tensorflow::SaverDef& def = op.saver_def();
    absl::string_view save_op = def.save_tensor_name();
    // TODO(team): Workaround due to difference between python and c++
    //  TensorFlow APIs.
    save_op = absl::StripSuffix(save_op, ":0");
    std::string tmp_file_name = GetTmpCheckpointFileName("save_checkpoint");
    res =
        RunOp({{def.filename_tensor_name(), tensorflow::Tensor(tmp_file_name)}},
              save_op)
            .Then([&tmp_file_name](Unit u) -> Result<StatusOr<absl::Cord>> {
              return Result(fcp::ReadFileToCord(tmp_file_name));
            })
            .Then(ExpectOk());
    DeleteTmpFile(tmp_file_name);
  }
  FCP_TRY(RunOp(op.after_save_op()));
  return res;
}

Result<Unit> TfSession::RestoreState(const CheckpointOp& op,
                                     const absl::Cord& checkpoint) {
  FCP_TRY(Ready());
  TracingSpan<RestoreFromCheckpoint> span(
      op.before_restore_op(),
      op.has_saver_def() ? op.saver_def().restore_op_name() : "",
      op.after_restore_op());
  FCP_TRY(RunOp(op.before_restore_op()));
  Result<Unit> res = Unit{};
  if (op.has_saver_def()) {
    const tensorflow::SaverDef& def = op.saver_def();
    std::string tmp_file_name = GetTmpCheckpointFileName("restore_checkpoint");
    res = Result(fcp::WriteCordToFile(tmp_file_name, checkpoint))
              .Then(ExpectOk())
              .Then([this, &def, &tmp_file_name](Unit u) -> Result<Unit> {
                return RunOp({{def.filename_tensor_name(),
                               tensorflow::Tensor(tmp_file_name)}},
                             def.restore_op_name());
              });
    DeleteTmpFile(tmp_file_name);
  }
  FCP_TRY(RunOp(op.after_restore_op()));
  return res;
}

Result<Unit> TfSession::RestoreState(const CheckpointOp& op,
                                     const NamedTensorList& restore_inputs) {
  FCP_TRY(Ready());
  TracingSpan<RestoreFromTensors> span(op.before_restore_op(),
                                       op.after_restore_op());
  if (op.has_saver_def()) {
    return TraceError<InvalidCheckpointOp>(
        "saver_def",
        "Cannot call RestoreState with a list of named tensors with a "
        "checkpoint op containing a SaverDef.");
  }
  FCP_TRY(RunOp(restore_inputs, op.before_restore_op()));
  return RunOp(op.after_restore_op());
}

std::string TfSession::GetTmpCheckpointFileName(absl::string_view name) {
  return ConcatPath(
      tmp_dir_, absl::StrCat(name, ProcessUniqueId::Next().value(), ".ckp"));
}

}  // namespace fcp
