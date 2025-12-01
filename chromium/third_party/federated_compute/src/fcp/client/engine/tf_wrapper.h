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
#ifndef FCP_CLIENT_ENGINE_TF_WRAPPER_H_
#define FCP_CLIENT_ENGINE_TF_WRAPPER_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/public/session.h"

namespace fcp {
namespace client {
namespace engine {

// A class to call into TensorFlow.
// All functions in this interface indicate errors as follows:
// - CANCELLED: interrupted execution
// - INVALID_ARGUMENT: TensorFlow error. The TensorFlow error code and message
//   are included in the Status message.
// - OUT_OF_RANGE: internal abortion, i.e. TensorFlow reporting the model
//   aborted execution.
// This class supports aborting ongoing calls, by polling the provided
// should_abort function.
class TensorFlowWrapper {
 public:
  static absl::StatusOr<std::unique_ptr<TensorFlowWrapper>> Create(
      const std::string& graph, const ::google::protobuf::Any& config_proto,
      std::function<bool()> should_abort,
      const InterruptibleRunner::TimingConfig& timing_config,
      LogManager* log_manager);

  // Utility method for creating a ConfigProto from an optionally
  // externally provided value, or from hardcoded defaults. This is a separate
  // method to aid with testing.
  static absl::StatusOr<::tensorflow::ConfigProto> InitializeConfigProto(
      const ::google::protobuf::Any& external_config_proto);

  ~TensorFlowWrapper();

  // Wrapper around TensorFlow's Session::Run method with full support for
  // feeds, fetches and target node names.
  // Returns OK, OUT_OF_RANGE, INVALID_ARGUMENT, or CANCELLED.
  absl::Status Run(
      const std::vector<std::pair<std::string, tensorflow::Tensor>>& inputs,
      const std::vector<std::string>& output_tensor_names,
      const std::vector<std::string>& target_node_names,
      std::vector<tensorflow::Tensor>* outputs);

  // Closes and releases the TensorFlow session. After this is called, no
  // further calls on this TensorFlowWrapper should be made. Subsequent calls to
  // CloseAndRelease() will have no effect.
  absl::Status CloseAndRelease();

 private:
  TensorFlowWrapper(std::unique_ptr<tensorflow::Session> session,
                    std::unique_ptr<InterruptibleRunner> interruptible_runner,
                    LogManager* log_manager)
      : session_(std::move(session)),
        interruptible_runner_(std::move(interruptible_runner)),
        session_closed_(false) {}

  // Converts a TensorFlow status to an absl::Status.
  //
  // Rule:
  // TensorFlow OK status -> absl OK status
  // TensorFlow OUT_OF_RANGE -> absl OUT_OF_RANGE status (this is TF indicating
  //   that the plan decided to abort, e.g. because of convergence)
  // Other TensorFlow status -> absl INVALID_ARGUMENT status with error
  // message being message_prefix + TensorFlow status code + error message.
  static absl::Status ToFcpStatus(const absl::Status& s,
                                  const std::string& message_prefix);

  std::unique_ptr<tensorflow::Session> session_;
  std::unique_ptr<InterruptibleRunner> interruptible_runner_;
  absl::Mutex session_lock_;
  bool session_closed_;
};

}  // namespace engine
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_ENGINE_TF_WRAPPER_H_
