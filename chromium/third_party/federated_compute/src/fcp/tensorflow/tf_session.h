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

#ifndef FCP_TENSORFLOW_TF_SESSION_H_
#define FCP_TENSORFLOW_TF_SESSION_H_

#include <filesystem>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "fcp/base/result.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/tensorflow/tracing_schema.h"
#include "fcp/tracing/tracing_span.h"
#include "tensorflow/core/public/session.h"

namespace fcp {

class TfSession {
 public:
  /**
   * Starts a tensorflow client session with the provided graph def
   * @param tmp_dir A directory in which to create tmp files used while saving
   *    or restoring checkpoints. This directory can be the same for multiple
   *    TfSessions created in the same process, even if they are running
   *    concurrently, but it must not be the same directory passed to a
   *    TfSession in a different process.
   * @param graph Serialized graph describing how to aggregate client updates
   *    into a global model. Must be parseable into a tesnorflow::GraphDef
   *    proto.
   */
  TfSession(const std::filesystem::path& tmp_dir, const absl::Cord& graph);
  TfSession(const std::filesystem::path& tmp_dir, absl::string_view graph);

  // TfSession is neither copyable nor movable.
  TfSession(const TfSession&) = delete;
  TfSession& operator=(const TfSession&) = delete;

  using NamedTensorList =
      std::vector<std::pair<std::string, tensorflow::Tensor>>;
  using NamedTensorMap = absl::flat_hash_map<std::string, tensorflow::Tensor>;

  // Returns Error if the TfSession is in a bad state (for example if the
  // provided GraphDef was invalid.) Allows failing fast while recording a
  // useful error for debugging.
  // If Ready() returns Error, all other methods will return Error as well.
  Result<Unit> Ready();

  // Run a single operation only if the operation is nonempty. The operation
  // must be present in the GraphDef that was provided in the constructor.
  Result<Unit> RunOp(absl::string_view op);

  // Returns a map of name, output tensor pairs for the outputs specified by
  // output_names.
  Result<std::unique_ptr<NamedTensorMap>> GetOutputs(
      std::unique_ptr<std::vector<std::string>> output_names);

  /**
   * Saves the current state of the session.
   * @param op Contains instructions for how to save the session state.
   * @return the state of the session as a serialized checkpoint.
   */
  Result<absl::Cord> SaveState(
      const google::internal::federated::plan::CheckpointOp& op);

  /**
   * Restores state into the session.
   * @param op Contains instructions for operations to run to restore the
   * state.
   * @param checkpoint Serialized tensorflow checkpoint that should be loaded
   * into the session.
   */
  Result<Unit> RestoreState(
      const google::internal::federated::plan::CheckpointOp& op,
      const absl::Cord& checkpoint);

  /**
   * Restores state into the session.
   * @param op Contains instructions for operations to run to restore the state.
   * saver_def must not be set on the op.
   * @param restore_inputs A collection of tensor variables that should be
   * loaded into the session.
   */
  Result<Unit> RestoreState(
      const google::internal::federated::plan::CheckpointOp& op,
      const NamedTensorList& restore_inputs);

 private:
  // Overload to allow providing inputs to operations.
  Result<Unit> RunOp(const NamedTensorList& inputs, absl::string_view op);
  std::string GetTmpCheckpointFileName(absl::string_view name);

  std::string tmp_dir_;
  std::unique_ptr<tensorflow::Session> session_;
  fcp::Status session_status_;
};

}  // namespace fcp

#endif  // FCP_TENSORFLOW_TF_SESSION_H_
