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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <future>  // NOLINT
#include <memory>
#include <optional>
#include <string>
#include <thread>  // NOLINT
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "fcp/base/monitoring.h"
#include "fcp/confidentialcompute/constants.h"
#include "fcp/confidentialcompute/tee_executor_value.h"
#include "fcp/protos/confidentialcompute/file_info.pb.h"
#include "federated_language/proto/array.pb.h"
#include "federated_language/proto/computation.pb.h"
#include "tensorflow_federated/cc/core/impl/executors/cardinalities.h"
#include "tensorflow_federated/cc/core/impl/executors/composing_executor.h"
#include "tensorflow_federated/cc/core/impl/executors/executor.h"
#include "tensorflow_federated/cc/core/impl/executors/threading.h"
#include "tensorflow_federated/proto/v0/executor.pb.h"

namespace fcp {
namespace confidential_compute {

namespace {

using ValueFuture = std::shared_future<absl::StatusOr<ExecutorValue>>;

std::shared_ptr<tensorflow_federated::OwnedValueId> ShareValueId(
    tensorflow_federated::OwnedValueId&& id) {
  return std::make_shared<tensorflow_federated::OwnedValueId>(std::move(id));
}

class ComposingTeeExecutor
    : public tensorflow_federated::ExecutorBase<ValueFuture> {
 public:
  explicit ComposingTeeExecutor(
      std::shared_ptr<Executor> server,
      std::vector<tensorflow_federated::ComposingChild> clients,
      std::optional<int32_t> threadpool_size = std::nullopt)
      : server_executor_(std::move(server)),
        client_executors_(std::move(clients)),
        thread_pool_(
            // Use a threadpool with CPU * 4 or the user specified
            // maximum.
            threadpool_size.value_or(std::thread::hardware_concurrency() * 4),
            ExecutorName()) {
    VLOG(2) << "thread pool size: "
            << threadpool_size.value_or(std::thread::hardware_concurrency() *
                                        4);
  }
  ~ComposingTeeExecutor() override {
    // Delete `OwnedValueId` and release them from the child executor before
    // destroying it.
    ClearTracked();
  }

 private:
  std::shared_ptr<Executor> server_executor_;
  std::vector<tensorflow_federated::ComposingChild> client_executors_;

  // IMPORTANT: The thread_pool_ must be the member of the class. This way the
  // thread_pool_ will be the first destructed, which will wait prevent new
  // work from being scheduled (closing the pool) and waiting for inflight
  // threads to finish, which might hold `this` pointers to the executor and
  // otherwise could cause issues.
  tensorflow_federated::ThreadPool thread_pool_;

  absl::string_view ExecutorName() final { return "ComposingTeeExecutor"; }

  absl::StatusOr<ValueFuture> CreateExecutorValue(
      const tensorflow_federated::v0::Value& value_pb) final {
    return tensorflow_federated::ReadyFuture(ExecutorValue(value_pb));
  }

  absl::StatusOr<std::shared_ptr<tensorflow_federated::OwnedValueId>>
  EmbedValue(const tensorflow_federated::v0::Value& value_to_embed,
             Executor& executor,
             std::optional<int32_t> total_client_executors = std::nullopt,
             std::optional<int32_t> executor_index = std::nullopt) {
    if (!value_to_embed.has_federated() ||
        value_to_embed.federated().type().placement().value().uri() !=
            tensorflow_federated::kClientsUri) {
      FCP_ASSIGN_OR_RETURN(tensorflow_federated::OwnedValueId handle,
                           executor.CreateValue(value_to_embed));
      return ShareValueId(std::move(handle));
    }
    if (!total_client_executors.has_value() || !executor_index.has_value()) {
      return absl::InternalError(
          "Expected total_client_executors and executor_index to be set");
    }
    if (*total_client_executors <= 0) {
      return absl::InvalidArgumentError(
          "Expected total_client_executors to be positive");
    }
    tensorflow_federated::v0::Value subvalue_to_embed;
    *subvalue_to_embed.mutable_federated()->mutable_type() =
        value_to_embed.federated().type();

    // Distribute the clients across the child executors. Each child
    // executor should get the same number of clients, with the exception of
    // the last child executor, which may get fewer clients if the number
    // of clients is not evenly divisible by the number of child executors.
    int num_clients_values = value_to_embed.federated().value().size();
    int num_clients_values_per_executors = std::ceil(
        static_cast<float>(num_clients_values) / *total_client_executors);
    int start_index = executor_index.value() * num_clients_values_per_executors;
    int end_index =
        std::min(num_clients_values, (executor_index.value() + 1) *
                                         num_clients_values_per_executors);
    for (int index = start_index; index < end_index; ++index) {
      *subvalue_to_embed.mutable_federated()->add_value() =
          value_to_embed.federated().value(index);
    }
    FCP_ASSIGN_OR_RETURN(tensorflow_federated::OwnedValueId handle,
                         executor.CreateValue(subvalue_to_embed));
    return ShareValueId(std::move(handle));
  }

  absl::StatusOr<std::shared_ptr<tensorflow_federated::OwnedValueId>>
  EmbedStruct(const std::shared_ptr<std::vector<ExecutorValue>> struct_elements,
              Executor& executor,
              std::optional<int32_t> total_client_executors = std::nullopt,
              std::optional<int32_t> executor_index = std::nullopt) {
    std::vector<std::shared_ptr<tensorflow_federated::OwnedValueId>>
        owned_child_ids;
    std::vector<tensorflow_federated::ValueId> child_ids;
    for (const ExecutorValue& element : *struct_elements) {
      FCP_ASSIGN_OR_RETURN(
          std::shared_ptr<tensorflow_federated::OwnedValueId> child_id,
          Embed(element, executor, total_client_executors, executor_index));
      owned_child_ids.push_back(child_id);
      child_ids.push_back(child_id->ref());
    }
    FCP_ASSIGN_OR_RETURN(tensorflow_federated::OwnedValueId handle,
                         executor.CreateStruct(child_ids));
    return ShareValueId(std::move(handle));
  }

  absl::StatusOr<std::shared_ptr<tensorflow_federated::OwnedValueId>>
  EmbedSelection(const ExecutorValue::Selection& selection, Executor& executor,
                 std::optional<int32_t> total_client_executors = std::nullopt,
                 std::optional<int32_t> executor_index = std::nullopt) {
    if (selection.source->type() == ExecutorValue::ValueType::STRUCT) {
      FCP_ASSIGN_OR_RETURN(const std::shared_ptr<std::vector<ExecutorValue>>
                               source_struct_elements,
                           selection.source->struct_elements());
      if (selection.index >= source_struct_elements->size()) {
        return absl::InvalidArgumentError(
            "Selection index is out of range for struct.");
      }
      return Embed((*source_struct_elements)[selection.index], executor,
                   total_client_executors, executor_index);
    }
    FCP_ASSIGN_OR_RETURN(
        std::shared_ptr<tensorflow_federated::OwnedValueId> owned_child_id,
        Embed(*selection.source, executor, total_client_executors,
              executor_index));
    FCP_ASSIGN_OR_RETURN(
        tensorflow_federated::OwnedValueId handle,
        executor.CreateSelection(owned_child_id->ref(), selection.index));
    return ShareValueId(std::move(handle));
  }

  absl::StatusOr<std::shared_ptr<tensorflow_federated::OwnedValueId>> Embed(
      const ExecutorValue& value, Executor& executor,
      std::optional<int32_t> total_client_executors = std::nullopt,
      std::optional<int32_t> executor_index = std::nullopt) {
    switch (value.type()) {
      case ExecutorValue::ValueType::VALUE: {
        FCP_ASSIGN_OR_RETURN(
            const tensorflow_federated::v0::Value value_to_embed,
            value.value());
        return EmbedValue(value_to_embed, executor, total_client_executors,
                          executor_index);
      }
      case ExecutorValue::ValueType::STRUCT: {
        FCP_ASSIGN_OR_RETURN(
            const std::shared_ptr<std::vector<ExecutorValue>> struct_elements,
            value.struct_elements());
        return EmbedStruct(struct_elements, executor, total_client_executors,
                           executor_index);
      }
      case ExecutorValue::ValueType::SELECTION: {
        FCP_ASSIGN_OR_RETURN(ExecutorValue::Selection selection,
                             value.selection());
        return EmbedSelection(selection, executor, total_client_executors,
                              executor_index);
      }
      default:
        return absl::InvalidArgumentError("Unsupported value type");
    }
  }

  absl::StatusOr<std::vector<ExecutorValue>> GetFederatedAggregateLeafResults(
      const federated_language::Type& accumulate_arg_type,
      const ExecutorValue& accumulate_arg,
      const federated_language::Type& accumulate_fn_type,
      const ExecutorValue& accumulate_fn) {
    // Call the leaf intrinsic function on each of the client child
    // executors and collect the results.
    std::vector<tensorflow_federated::OwnedValueId> leaf_result_handles;
    std::vector<ExecutorValue> leaf_result_vals;

    // Prepare the leaf intrinsic function to call on each of the client
    // child executors. This function will have the following signature:
    // (accumulate_arg, accumulate_fn) -> leaf_result.
    tensorflow_federated::v0::Value leaf_intrinsic_pb;
    leaf_intrinsic_pb.mutable_computation()->mutable_intrinsic()->set_uri(
        kComposedTeeLeafUri);
    federated_language::FunctionType* leaf_intrinsic_type =
        leaf_intrinsic_pb.mutable_computation()
            ->mutable_type()
            ->mutable_function();
    *leaf_intrinsic_type->mutable_parameter()
         ->mutable_struct_()
         ->add_element()
         ->mutable_value() = accumulate_arg_type;
    *leaf_intrinsic_type->mutable_parameter()
         ->mutable_struct_()
         ->add_element()
         ->mutable_value() = accumulate_fn_type;
    *leaf_intrinsic_type->mutable_result() =
        accumulate_fn_type.function().result();
    ExecutorValue leaf_intrinsic_value(leaf_intrinsic_pb);

    // Use separate loops for CreateCall and Materialize to allow parallel
    // execution across the client child executors.
    for (int32_t i = 0; i < client_executors_.size(); ++i) {
      const std::shared_ptr<Executor> child = client_executors_[i].executor();

      FCP_ASSIGN_OR_RETURN(
          std::shared_ptr<tensorflow_federated::OwnedValueId>
              leaf_intrinsic_handle,
          Embed(leaf_intrinsic_value, *child, client_executors_.size(), i));

      std::vector<ExecutorValue> leaf_arg_vector = {accumulate_arg,
                                                    accumulate_fn};
      ExecutorValue leaf_arg_val(std::make_shared<std::vector<ExecutorValue>>(
          std::move(leaf_arg_vector)));
      FCP_ASSIGN_OR_RETURN(
          std::shared_ptr<tensorflow_federated::OwnedValueId> leaf_arg_handle,
          Embed(leaf_arg_val, *child, client_executors_.size(), i));

      FCP_ASSIGN_OR_RETURN(tensorflow_federated::OwnedValueId leaf_call_handle,
                           child->CreateCall(leaf_intrinsic_handle->ref(),
                                             leaf_arg_handle->ref()));

      leaf_result_handles.push_back(std::move(leaf_call_handle));
    }

    for (int32_t i = 0; i < client_executors_.size(); ++i) {
      const std::shared_ptr<Executor> child = client_executors_[i].executor();

      tensorflow_federated::v0::Value leaf_result_pb;
      FCP_RETURN_IF_ERROR(
          child->Materialize(leaf_result_handles[i].ref(), &leaf_result_pb));

      if (!leaf_result_pb.has_computation() ||
          !leaf_result_pb.computation().has_data()) {
        return absl::InvalidArgumentError(
            "Expected client child executor to return a data computation "
            "value");
      }

      leaf_result_vals.push_back(ExecutorValue(leaf_result_pb));
    }
    return leaf_result_vals;
  }

  absl::StatusOr<ExecutorValue> GetFederatedAggregateRootResult(
      const federated_language::Type& combined_partial_aggregate_input_type,
      const ExecutorValue& combined_partial_aggregate_inputs,
      const federated_language::Type& remaining_input_type,
      const ExecutorValue& remaining_input,
      const federated_language::Type& report_fn_type,
      const ExecutorValue& report_fn) {
    // Prepare the root intrinsic function to call on the server executor.
    // This function will have the following signature:
    // ([[accumulate_fn_result[kPartialAggregateOutputIndex] for each leaf
    // executor], partial_report_arg], report_fn) -> report_fn_result.
    tensorflow_federated::v0::Value root_intrinsic_pb;
    root_intrinsic_pb.mutable_computation()->mutable_intrinsic()->set_uri(
        kComposedTeeRootUri);
    federated_language::FunctionType* root_intrinsic_type =
        root_intrinsic_pb.mutable_computation()
            ->mutable_type()
            ->mutable_function();
    federated_language::StructType* full_report_arg_type =
        root_intrinsic_type->mutable_parameter()
            ->mutable_struct_()
            ->add_element()
            ->mutable_value()
            ->mutable_struct_();
    *full_report_arg_type->add_element()->mutable_value() =
        combined_partial_aggregate_input_type;
    *full_report_arg_type->add_element()->mutable_value() =
        remaining_input_type;
    *root_intrinsic_type->mutable_parameter()
         ->mutable_struct_()
         ->add_element()
         ->mutable_value() = report_fn_type;
    *root_intrinsic_type->mutable_result() = report_fn_type.function().result();

    // Call the root intrinsic function on the server child executor.
    FCP_ASSIGN_OR_RETURN(
        std::shared_ptr<tensorflow_federated::OwnedValueId>
            root_intrinsic_handle,
        Embed(ExecutorValue(std::move(root_intrinsic_pb)), *server_executor_));
    std::vector<ExecutorValue> root_arg_inputs_vector = {
        combined_partial_aggregate_inputs, remaining_input};
    ExecutorValue root_arg_inputs_val(
        std::make_shared<std::vector<ExecutorValue>>(
            std::move(root_arg_inputs_vector)));
    std::vector<ExecutorValue> root_arg_vector = {root_arg_inputs_val,
                                                  report_fn};
    ExecutorValue root_arg_val(std::make_shared<std::vector<ExecutorValue>>(
        std::move(root_arg_vector)));
    FCP_ASSIGN_OR_RETURN(
        std::shared_ptr<tensorflow_federated::OwnedValueId> root_arg_handle,
        Embed(root_arg_val, *server_executor_));

    FCP_ASSIGN_OR_RETURN(
        tensorflow_federated::OwnedValueId root_call_handle,
        server_executor_->CreateCall(root_intrinsic_handle->ref(),
                                     root_arg_handle->ref()));

    tensorflow_federated::v0::Value root_result_pb;
    FCP_RETURN_IF_ERROR(
        server_executor_->Materialize(root_call_handle.ref(), &root_result_pb));

    if (!root_result_pb.has_computation() ||
        !root_result_pb.computation().has_data()) {
      return absl::InvalidArgumentError(
          "Expected server child executor to return a data computation "
          "value");
    }
    return ExecutorValue(std::move(root_result_pb));
  }

  struct PartitionedLeafResults {
    // A CLIENTS-placed federated value containing all the pre-aggregate outputs
    // from each leaf executor. We use a federated value to encode this
    // information instead of a struct because we may need to divvy it across
    // multiple client child executors when executing a future composed_tee
    // call.
    ExecutorValue combined_pre_aggregate_outputs;

    // A struct containing all the partial aggregate outputs from each leaf
    // executor.
    ExecutorValue combined_partial_aggregate_outputs;

    // The type of the combined_partial_aggregate_outputs field.
    federated_language::Type combined_partial_aggregate_output_type;
  };

  absl::StatusOr<PartitionedLeafResults> PartitionLeafResults(
      const federated_language::Type& accumulate_fn_type,
      absl::Span<const ExecutorValue> leaf_result_vals) {
    // Retrieve the portion of the leaf results representing the pre-aggregate
    // outputs.
    tensorflow_federated::v0::Value combined_pre_aggregate_outputs_pb;
    federated_language::FederatedType* combined_pre_aggregate_outputs_pb_type =
        combined_pre_aggregate_outputs_pb.mutable_federated()->mutable_type();
    combined_pre_aggregate_outputs_pb_type->set_all_equal(false);
    combined_pre_aggregate_outputs_pb_type->mutable_placement()
        ->mutable_value()
        ->mutable_uri()
        ->assign(tensorflow_federated::kClientsUri.data(),
                 tensorflow_federated::kClientsUri.size());
    *combined_pre_aggregate_outputs_pb_type->mutable_member() =
        accumulate_fn_type.function()
            .result()
            .struct_()
            .element(kPreAggregateOutputIndex)
            .value();
    for (int32_t i = 0; i < client_executors_.size(); i++) {
      FCP_ASSIGN_OR_RETURN(
          *combined_pre_aggregate_outputs_pb.mutable_federated()->add_value(),
          ExecutorValue(std::make_shared<ExecutorValue>(leaf_result_vals[i]),
                        kPreAggregateOutputIndex)
              .resolve_to_value());
    }

    // Retrieve the portion of the leaf results representing the partial
    // aggregate outputs.
    federated_language::Type combined_partial_aggregate_output_type;
    std::vector<ExecutorValue> combined_partial_aggregate_outputs;
    for (int32_t i = 0; i < client_executors_.size(); i++) {
      *combined_partial_aggregate_output_type.mutable_struct_()
           ->add_element()
           ->mutable_value() = accumulate_fn_type.function()
                                   .result()
                                   .struct_()
                                   .element(kPartialAggregateOutputIndex)
                                   .value();
      combined_partial_aggregate_outputs.push_back(
          ExecutorValue(std::make_shared<ExecutorValue>(leaf_result_vals[i]),
                        kPartialAggregateOutputIndex));
    }

    return PartitionedLeafResults{
        .combined_pre_aggregate_outputs =
            ExecutorValue(std::move(combined_pre_aggregate_outputs_pb)),
        .combined_partial_aggregate_outputs =
            ExecutorValue(std::make_shared<std::vector<ExecutorValue>>(
                std::move(combined_partial_aggregate_outputs))),
        .combined_partial_aggregate_output_type =
            combined_partial_aggregate_output_type,
    };
  }

  absl::StatusOr<ValueFuture> CreateCall(
      ValueFuture function, std::optional<ValueFuture> argument) final {
    return ThreadRun(
        [function = std::move(function), argument = std::move(argument),
         this]() -> absl::StatusOr<ExecutorValue> {
          FCP_ASSIGN_OR_RETURN(ExecutorValue fn_value,
                               tensorflow_federated::Wait(function));
          FCP_ASSIGN_OR_RETURN(tensorflow_federated::v0::Value fn_pb,
                               fn_value.value());
          if (!fn_pb.has_computation() ||
              !fn_pb.computation().has_intrinsic()) {
            return absl::InvalidArgumentError("Function has unexpected form");
          }

          std::string uri(fn_pb.computation().intrinsic().uri());
          if (uri != kComposedTeeUri) {
            return absl::InvalidArgumentError("Unsupported intrinsic " + uri);
          }

          if (!argument.has_value()) {
            return absl::InvalidArgumentError("Missing required argument");
          }
          FCP_ASSIGN_OR_RETURN(ExecutorValue arg_value,
                               tensorflow_federated::Wait(argument.value()));
          FCP_ASSIGN_OR_RETURN(const std::shared_ptr<std::vector<ExecutorValue>>
                                   arg_struct_elements,
                               arg_value.struct_elements());
          if (arg_struct_elements->size() != 4) {
            return absl::InvalidArgumentError("Expected argument of size 4");
          }
          const ExecutorValue& accumulate_arg = (*arg_struct_elements)[0];
          const ExecutorValue& remaining_report_arg = (*arg_struct_elements)[1];
          const ExecutorValue& accumulate_fn = (*arg_struct_elements)[2];
          const ExecutorValue& report_fn = (*arg_struct_elements)[3];

          const federated_language::StructType& fn_arg_types =
              fn_pb.computation().type().function().parameter().struct_();
          const federated_language::Type& accumulate_arg_type =
              fn_arg_types.element(0).value();
          const federated_language::Type& remaining_report_arg_type =
              fn_arg_types.element(1).value();
          const federated_language::Type& accumulate_fn_type =
              fn_arg_types.element(2).value();
          const federated_language::Type& report_fn_type =
              fn_arg_types.element(3).value();

          if (!accumulate_fn_type.has_function()) {
            return absl::InvalidArgumentError(
                "Expected accumulate_fn_type to be a function");
          }
          if (!accumulate_fn_type.function().result().has_struct_() ||
              accumulate_fn_type.function().result().struct_().element_size() !=
                  2) {
            return absl::InvalidArgumentError(
                "Expected accumulate_fn_type to have a struct result with two "
                "elements");
          }

          // Collect the results from calling accumulate_fn on each of the
          // client child executors.
          FCP_ASSIGN_OR_RETURN(std::vector<ExecutorValue> leaf_result_vals,
                               GetFederatedAggregateLeafResults(
                                   accumulate_arg_type, accumulate_arg,
                                   accumulate_fn_type, accumulate_fn));

          // Partition the leaf results into the pre-aggregate outputs and the
          // partial aggregate outputs.
          FCP_ASSIGN_OR_RETURN(
              PartitionedLeafResults partitioned_leaf_results,
              PartitionLeafResults(accumulate_fn_type, leaf_result_vals));

          // Get the result of calling report_fn on the server child executor.
          FCP_ASSIGN_OR_RETURN(
              ExecutorValue root_result,
              GetFederatedAggregateRootResult(
                  partitioned_leaf_results
                      .combined_partial_aggregate_output_type,
                  partitioned_leaf_results.combined_partial_aggregate_outputs,
                  remaining_report_arg_type, remaining_report_arg,
                  report_fn_type, report_fn));

          // Construct the final result of the composed_tee call. This will be a
          // struct with two elements:
          // - The combined pre-aggregate outputs from each leaf executor.
          // - The root_result from the server child executor.
          std::vector<ExecutorValue> final_result_vector = {
              partitioned_leaf_results.combined_pre_aggregate_outputs,
              root_result};
          return ExecutorValue(std::make_shared<std::vector<ExecutorValue>>(
              std::move(final_result_vector)));
        },
        &thread_pool_);
  }

  absl::StatusOr<ValueFuture> CreateStruct(
      std::vector<ValueFuture> elements) final {
    return Map(
        std::move(elements),
        [](std::vector<ExecutorValue> elements)
            -> absl::StatusOr<ExecutorValue> {
          return ExecutorValue(std::make_shared<std::vector<ExecutorValue>>(
              std::move(elements)));
        },
        &thread_pool_);
  }

  absl::StatusOr<ValueFuture> CreateSelection(ValueFuture value,
                                              uint32_t index) final {
    return ThreadRun(
        [value = std::move(value), index]() -> absl::StatusOr<ExecutorValue> {
          FCP_ASSIGN_OR_RETURN(ExecutorValue source_val,
                               tensorflow_federated::Wait(value));
          return ExecutorValue(std::make_shared<ExecutorValue>(source_val),
                               index);
        },
        &thread_pool_);
  }

  absl::Status Materialize(ValueFuture value_fut,
                           tensorflow_federated::v0::Value* value_pb) override {
    FCP_ASSIGN_OR_RETURN(ExecutorValue value,
                         tensorflow_federated::Wait(value_fut));
    FCP_ASSIGN_OR_RETURN(*value_pb, value.resolve_to_value());
    return absl::OkStatus();
  }
};

}  // namespace

std::shared_ptr<tensorflow_federated::Executor> CreateComposingTeeExecutor(
    std::shared_ptr<tensorflow_federated::Executor> server,
    std::vector<tensorflow_federated::ComposingChild> clients) {
  return std::make_shared<ComposingTeeExecutor>(std::move(server),
                                                std::move(clients));
}

}  // namespace confidential_compute
}  // namespace fcp
