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
#include "fcp/client/engine/tflite_wrapper.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/engine/caching_error_reporter.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "tensorflow/core/public/version.h"
#include "tensorflow/lite/c/c_api_types.h"
#include "tensorflow/lite/delegates/flex/delegate.h"
#include "tensorflow/lite/delegates/flex/util.h"
#include "tensorflow/lite/delegates/utils/simple_delegate.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/interpreter_builder.h"
#include "tensorflow/lite/interpreter_options.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model_builder.h"
#include "tensorflow/lite/string_util.h"

namespace fcp {
namespace client {
namespace engine {

namespace {

absl::Status AssignStringInput(int index, const std::string& value,
                               tflite::Interpreter* interpreter) {
  TfLiteTensor* tensor = interpreter->tensor(index);
  if (tensor->type != kTfLiteString) {
    return absl::InvalidArgumentError("Input tensor is not a string tensor.");
  }

  tflite::DynamicBuffer buf;
  buf.AddString(value.data(), value.length());
  buf.WriteToTensor(tensor, nullptr);
  return absl::OkStatus();
}

// Holds an initialized tflite::Interpreter instance, as well as the various
// objects that interpreter depends on, and which should be kept alive for at
// least as long as the interpreter itself.
//
// Note that member fields are destroyed in the reverse order they're defined
// in, so the interpreter is destroyed before the delegate is, etc.
struct TfLiteInterpreterWithDeps {
  std::unique_ptr<tflite::FlatBufferModel> model;
  std::unique_ptr<CachingErrorReporter> error_reporter;
  tflite::TfLiteDelegateUniquePtr delegate;
  std::unique_ptr<tflite::Interpreter> interpreter;
};

// Initializes the interpreter with the given model, inputs, and output names
// and returns the initialized instance as well as its dependencies.
absl::StatusOr<TfLiteInterpreterWithDeps> InitializeInterpreter(
    const std::string& model,
    const absl::flat_hash_map<std::string, std::string>& inputs,
    std::vector<std::string> output_names,
    const TfLiteInterpreterOptions& interpreter_options, int32_t num_threads) {
  std::unique_ptr<tflite::FlatBufferModel> flat_buffer_model =
      tflite::FlatBufferModel::BuildFromBuffer(model.c_str(), model.size());
  if (flat_buffer_model == nullptr) {
    return absl::InvalidArgumentError("Failed to build FlatBufferModel.");
  }
  // The training delegate needs to be created before the interpreter.
  auto delegate = tflite::FlexDelegate::Create();
  auto error_reporter = std::make_unique<CachingErrorReporter>();
  auto interpreter = std::make_unique<tflite::Interpreter>();

  std::unique_ptr<tflite::ops::builtin::BuiltinOpResolver> resolver;
  if (interpreter_options.use_builtin_op_resolver_with_default_delegates) {
    resolver = std::make_unique<tflite::ops::builtin::BuiltinOpResolver>();
  } else {
    resolver = std::make_unique<
        tflite::ops::builtin::BuiltinOpResolverWithoutDefaultDelegates>();
  }

  if (tflite::InterpreterBuilder(flat_buffer_model->GetModel(), *resolver,
                                 error_reporter.get())(&interpreter) !=
      kTfLiteOk) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to initiate interpreter: %s",
                        error_reporter->GetFirstErrorMessage()));
  }
  interpreter->SetNumThreads(num_threads);
  if (interpreter->ModifyGraphWithDelegate(delegate.get()) != kTfLiteOk) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to modify graph with FlexDelegate: %s",
                        error_reporter->GetFirstErrorMessage()));
  }
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to allocate tensors: %s",
                        error_reporter->GetFirstErrorMessage()));
  }
  interpreter->SetCancellationFunction(delegate->data_,
                                       tflite::FlexDelegate::HasCancelled);
  for (const auto& input : interpreter->inputs()) {
    std::string key = interpreter->GetInputName(input);
    if (inputs.find(key) == inputs.end()) {
      return absl::InvalidArgumentError("Unexpected input tensor.");
    }
    FCP_RETURN_IF_ERROR(
        AssignStringInput(input, inputs.at(key), interpreter.get()));
  }

  return TfLiteInterpreterWithDeps{.model = std::move(flat_buffer_model),
                                   .error_reporter = std::move(error_reporter),
                                   .delegate = std::move(delegate),
                                   .interpreter = std::move(interpreter)};
}

absl::Status ConvertTfLiteStatusInternal(TfLiteStatus status,
                                         TfLiteDelegate& delegate,
                                         CachingErrorReporter& error_reporter) {
  switch (status) {
    case kTfLiteOk:
      return absl::OkStatus();
    case kTfLiteError: {
      // TfLite doesn't differentiate the error type when the training is
      // cancelled or an error happened during training. It also doesn't
      // distinguish different error types thrown by Tensorflow. Therefore, we
      // need to check whether the training was cancelled, and record the error
      // message from the ErrorReporter.
      if (tflite::FlexDelegate::HasCancelled(delegate.data_)) {
        return absl::CancelledError("Training is cancelled.");
      }
      std::string error = error_reporter.GetFirstErrorMessage();
      if (error.empty()) {
        return absl::InvalidArgumentError("Empty error messages returned.");
      }
      // Use the first error we encountered.
      return absl::InvalidArgumentError(error);
    }
    case kTfLiteDelegateError:
      return absl::InvalidArgumentError("TfLite delegate error.");
    case kTfLiteApplicationError:
      return absl::InvalidArgumentError(
          "An error in applying a delegate due to incompatibility between "
          "runtime and delegate");
    case kTfLiteDelegateDataNotFound:
      return absl::InvalidArgumentError(
          "Serialized delegate data not being found");
    case kTfLiteDelegateDataWriteError:
      return absl::InvalidArgumentError(
          "Data-writing issues in delegate serialization");
    case kTfLiteDelegateDataReadError:
      return absl::InvalidArgumentError(
          "Data-reading issues in delegate serialization.");
    case kTfLiteUnresolvedOps:
      return absl::InvalidArgumentError(
          "The TF Lite model has ops that cannot be resolved at runtime.");
    default:
      return absl::InternalError("Unexpected TfLiteStatus.");
  }
}

absl::StatusOr<OutputTensors> ConstructOutputsInternal(
    tflite::Interpreter& interpreter,
    const std::vector<std::string>& output_names) {
  if (interpreter.outputs().size() != output_names.size()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("The number of output tensors is wrong. Expected: %d, "
                        "Returned by TFLite interpreter: %d",
                        output_names.size(), interpreter.outputs().size()));
  }
  OutputTensors output_tensors;
  // The order of the output tensors should match the order of output tensor
  // names.
  for (int output_tensor_index : interpreter.outputs()) {
    auto tensor = tflite::flex::CreateTfTensorFromTfLiteTensor(
        interpreter.tensor(output_tensor_index));
    if (!tensor.ok()) {
#if TF_GRAPH_DEF_VERSION < 1467
      return absl::InvalidArgumentError(tensor.status().error_message());
#else
      return absl::InvalidArgumentError(tensor.status().message());
#endif
    }
    output_tensors.output_tensors.push_back(*tensor);
  }
  output_tensors.output_tensor_names = output_names;
  return output_tensors;
}

// Runs the given TFLite model by first initializing an interpreter and then
// invoking it right away, returning the resulting output tensors.
absl::StatusOr<OutputTensors> RunTfLiteModelInternal(
    const std::string& model,
    const absl::flat_hash_map<std::string, std::string>& inputs,
    std::vector<std::string> output_names,
    const TfLiteInterpreterOptions& interpreter_options, int32_t num_threads,
    std::atomic<tflite::FlexDelegate*>& delegate_raw_ptr_holder) {
  FCP_ASSIGN_OR_RETURN(TfLiteInterpreterWithDeps interpreter_with_deps,
                       InitializeInterpreter(model, inputs, output_names,
                                             interpreter_options, num_threads));

  // Store a pointer to the delegate in the holder we were given. This will
  // allow the caller to cancel the invocation while it is running.
  delegate_raw_ptr_holder.store(static_cast<tflite::FlexDelegate*>(
      interpreter_with_deps.delegate->data_));

  // Invoke the model and convert the TFLite status to an absl status.
  TfLiteStatus tflite_result = interpreter_with_deps.interpreter->Invoke();
  absl::Status result = ConvertTfLiteStatusInternal(
      tflite_result, *interpreter_with_deps.delegate,
      *interpreter_with_deps.error_reporter);

  // Clear the delegate pointer *before* we return, since the delegate is about
  // to go out of scope and therefore will be destroyed.
  delegate_raw_ptr_holder.store(nullptr);
  if (!result.ok()) {
    return result;
  }
  // Extract output tensors from the interpreter's state, and turn them into
  // tensorflow::Tensors we can return.
  return ConstructOutputsInternal(*interpreter_with_deps.interpreter,
                                  output_names);
}

}  // anonymous namespace

absl::StatusOr<OutputTensors> RunTfLiteModelThreadSafe(
    const std::string& model, std::function<bool()> should_abort,
    const InterruptibleRunner::TimingConfig& timing_config,
    LogManager* log_manager,
    std::unique_ptr<absl::flat_hash_map<std::string, std::string>> inputs,
    std::vector<std::string> output_names,
    const TfLiteInterpreterOptions& interpreter_options, int32_t num_threads) {
  // Create an InterruptibleRunner to execute TF calls in a background thread,
  // allowing us to abort them if need be.
  auto runner = std::make_unique<InterruptibleRunner>(
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

  std::atomic<tflite::FlexDelegate*> delegate_raw_ptr;

  OutputTensors output_tensors;
  FCP_RETURN_IF_ERROR(runner->Run(
      [model, inputs_ = *inputs, output_names, interpreter_options, num_threads,
       output_tensors_ = &output_tensors, &delegate_raw_ptr]() mutable {
        // Run the model, and if successful then copy its output tensors so we
        // can return them.
        FCP_ASSIGN_OR_RETURN(
            *output_tensors_,
            RunTfLiteModelInternal(model, inputs_, output_names,
                                   interpreter_options, num_threads,
                                   delegate_raw_ptr));
        return absl::OkStatus();
      },
      /* abort_function=*/
      [&delegate_raw_ptr]() {
        auto local_delegate_raw_ptr = delegate_raw_ptr.load();
        if (local_delegate_raw_ptr != nullptr) {
          local_delegate_raw_ptr->Cancel();
        }
      }));
  return output_tensors;
}

}  // namespace engine
}  // namespace client
}  // namespace fcp
