/*
 * Copyright 2019 Google LLC
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

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/engine/plan_engine_helpers.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/public/session.h"
#include "tensorflow/core/public/session_options.h"

namespace fcp {
namespace client {
namespace engine {

using ::google::protobuf::Any;

// If `external_config_proto` contains a non-empty config proto, use that.
// Otherwise initializes a config proto from a set of defaults.
absl::StatusOr<tensorflow::ConfigProto>
TensorFlowWrapper::InitializeConfigProto(const Any& external_config_proto) {
  // Previously, we specified a hardcoded set of options in the ConfigProto by
  // default. However, if a non-empty ConfigProto is now provided as a
  // parameter, then we should use it as-is, without overriding any of the
  // options (otherwise we prevent the caller from having control over the
  // parameters we set by default).
  if (external_config_proto.ByteSizeLong() > 0) {
    // Unpack the external_config_proto parameter if one is provided. In this
    // case it must be a packed ConfigProto (anything else is an error).
    // Accordingly, UnpackTo will return false if parsing fails or if the Any is
    // not of a compatible type.
    tensorflow::ConfigProto unpacked_config_proto;
    if (!external_config_proto.UnpackTo(&unpacked_config_proto)) {
      return absl::InvalidArgumentError("Could not parse ConfigProto.");
    }
    if (unpacked_config_proto.ByteSizeLong() > 0) {
      // The caller-provided, unpacked ConfigProto was not empty, so we use it
      // in the SessionOptions and we do not specify our default config options
      // anymore.
      return unpacked_config_proto;
    }
    // We purposely fall through to the next block if the unpacked_config_proto
    // was empty.
  }

  // Only if the provided ConfigProto was empty (or if none was provided) do we
  // still set hardcoded options (this is our "old" behavior, equivalent to what
  // we did before we supported caller-specified ConfigProtos).
  //
  // WARNING: If the need for tuning configuration options further arises again
  // in the future, we ideally shouldn't update any of the hardcoded ConfigProto
  // values here anymore. Instead, we should expect our callers to specify any
  // ConfigProto values they want to use. We only maintain this block of code
  // for compatibility with callers that don't provide any ConfigProto at all
  // (yet).
  //
  tensorflow::ConfigProto config_proto;
  config_proto.mutable_graph_options()->set_place_pruned_graph(true);
  auto mutable_experimental = config_proto.mutable_experimental();
  mutable_experimental->set_optimize_for_static_graph(true);
  mutable_experimental->set_disable_output_partition_graphs(true);
  return config_proto;
}

absl::StatusOr<std::unique_ptr<TensorFlowWrapper>> TensorFlowWrapper::Create(
    const std::string& graph, const Any& config_proto,
    std::function<bool()> should_abort,
    const InterruptibleRunner::TimingConfig& timing_config,
    LogManager* log_manager) {
  // Create a tensorflow::Session.
  tensorflow::Session* session_ptr;
  std::unique_ptr<tensorflow::Session> session;
  tensorflow::SessionOptions session_options;
  FCP_ASSIGN_OR_RETURN(session_options.config,
                       InitializeConfigProto(config_proto));

  absl::Status status = tensorflow::NewSession(session_options, &session_ptr);
  if (!status.ok()) {
    return ToFcpStatus(status, "Error in tensorflow::NewSession()");
  }
  session = absl::WrapUnique(session_ptr);

  // Parse GraphDef.
  tensorflow::GraphDef graph_def;
  bool parse_result = graph_def.ParseFromString(graph);
  if (parse_result == false) {
    return absl::InvalidArgumentError("Could not parse GraphDef.");
  }
  // Load graph.
  status = session->Create(std::move(graph_def));
  if (!status.ok()) {
    return ToFcpStatus(status, "Error in Session::Create()");
  }

  // Create an InterruptibleRunner to execute TF calls in a background thread,
  // allowing us to abort them if need be.
  auto interruptible_runner = std::make_unique<InterruptibleRunner>(
      log_manager, should_abort, timing_config,
      InterruptibleRunner::DiagnosticsConfig{
          .interrupted =
              ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_TF_EXECUTION,
          .interrupt_timeout = ProdDiagCode::
              BACKGROUND_TRAINING_INTERRUPT_TF_EXECUTION_TIMED_OUT,
          .interrupted_extended = ProdDiagCode::
              BACKGROUND_TRAINING_INTERRUPT_TF_EXTENDED_EXECUTION_COMPLETED,
          .interrupt_timeout_extended = ProdDiagCode::
              BACKGROUND_TRAINING_INTERRUPT_TF_EXTENDED_EXECUTION_TIMED_OUT});
  auto wrapper = absl::WrapUnique(new TensorFlowWrapper(
      std::move(session), std::move(interruptible_runner), log_manager));
  return wrapper;
}

TensorFlowWrapper::~TensorFlowWrapper() { FCP_CHECK(CloseAndRelease().ok()); }

absl::Status TensorFlowWrapper::ToFcpStatus(const absl::Status& s,
                                            const std::string& message_prefix) {
  if (s.ok()) {
    return absl::OkStatus();
  } else if (s.code() == absl::StatusCode::kOutOfRange) {
    return absl::OutOfRangeError("");
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat(message_prefix, ": ", s.ToString()));
  }
}

absl::Status TensorFlowWrapper::Run(
    const std::vector<std::pair<std::string, tensorflow::Tensor>>& inputs,
    const std::vector<std::string>& output_tensor_names,
    const std::vector<std::string>& target_node_names,
    std::vector<tensorflow::Tensor>* outputs) {
  FCP_CHECK(!session_closed_) << "Run() called after session close!";

  auto tensorflow_runnable = [&inputs, &output_tensor_names, &target_node_names,
                              &outputs, this]() -> absl::Status {
    absl::Status status = this->session_->Run(inputs, output_tensor_names,
                                              target_node_names, outputs);
    if (!status.ok()) {
      return ToFcpStatus(status, "Error in Session::Run()");
    }
    return absl::OkStatus();
  };
  auto abort_tensorflow = [this]() {
    absl::MutexLock _(&session_lock_);
    // Errors from Close() are expected when interrupting ongoing calls. We
    // don't call CloseAndRelease() here because that would free the TensorFlow
    // session while other TensorFlow worker threads may still be using it.
    session_->Close().IgnoreError();
    session_closed_ = true;
  };
  return interruptible_runner_->Run(tensorflow_runnable, abort_tensorflow);
}

absl::Status TensorFlowWrapper::CloseAndRelease() {
  absl::MutexLock _(&session_lock_);
  // If the TensorFlow session hasn't been closed yet, close it.
  if (!session_closed_) {
    FCP_ENGINE_RETURN_IF_ERROR(
        ToFcpStatus(session_->Close(), "Could not close TF session"));
    session_closed_ = true;
  }
  // If the TensorflowSession hasn't been released yet, release it.
  if (session_) {
    session_.reset();
  }
  return absl::OkStatus();
}

}  // namespace engine
}  // namespace client
}  // namespace fcp
