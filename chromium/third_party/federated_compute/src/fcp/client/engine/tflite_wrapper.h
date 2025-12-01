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
#ifndef FCP_CLIENT_ENGINE_TFLITE_WRAPPER_H_
#define FCP_CLIENT_ENGINE_TFLITE_WRAPPER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "tensorflow/core/framework/tensor.h"

namespace fcp {
namespace client {
namespace engine {

struct OutputTensors {
  std::vector<std::string> output_tensor_names;
  std::vector<tensorflow::Tensor> output_tensors;
};

// Options for TFLite interpreter.
struct TfLiteInterpreterOptions {
  // When true, TFLite uses dynamic tensor allocation and release tensors that
  // are no longer needed.
  bool ensure_dynamic_tensors_are_released = false;
  // When the threshold is zero, dynamic allocation is not enabled for any
  // tensor.
  int32_t large_tensor_threshold_for_dynamic_allocation = 0;
  // Whether to disable the graph-reordering optimization that clusters delegate
  // ops together.
  bool disable_delegate_clustering = false;
  // Whether to use TFLite's BuiltinOpResolver (as opposed to
  // BuiltinOpResolverWithoutDefaultDelegates).
  bool use_builtin_op_resolver_with_default_delegates = false;
};

// This method does the whole TFLite interpreter initialization *as well
// as* invocation in one go. It only invokes TF Lite APIs from a single thread
// (except for those explicitly marked as thread-safe, such as
// FlexDelegate::Cancel).
absl::StatusOr<OutputTensors> RunTfLiteModelThreadSafe(
    const std::string& model, std::function<bool()> should_abort,
    const InterruptibleRunner::TimingConfig& timing_config,
    LogManager* log_manager,
    std::unique_ptr<absl::flat_hash_map<std::string, std::string>> inputs,
    std::vector<std::string> output_names,
    const TfLiteInterpreterOptions& interpreter_options, int32_t num_threads);

}  // namespace engine
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_ENGINE_TFLITE_WRAPPER_H_
