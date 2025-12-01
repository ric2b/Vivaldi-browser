/*
 * Copyright 2023 Google LLC
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

#include "fcp/client/engine/example_query_plan_engine.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "google/type/datetime.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/civil_time.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/converters.h"
#include "fcp/client/engine/common.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/engine/privacy_id_utils.h"
#include "fcp/client/event_time_range.pb.h"
#include "fcp/client/example_iterator_query_recorder.h"
#include "fcp/client/example_query_result.pb.h"
#include "fcp/client/opstats/opstats_logger.h"
#include "fcp/client/simple_task_environment.h"
#include "fcp/client/tensorflow/tensorflow_runner.h"
#include "fcp/confidentialcompute/constants.h"
#include "fcp/protos/data_type.pb.h"
#include "fcp/protos/plan.pb.h"
#include "tensorflow_federated/cc/core/impl/aggregation/core/tensor.h"
#include "tensorflow_federated/cc/core/impl/aggregation/core/tensor.pb.h"
#include "tensorflow_federated/cc/core/impl/aggregation/core/tensor_shape.h"
#include "tensorflow_federated/cc/core/impl/aggregation/protocol/checkpoint_builder.h"
#include "tensorflow_federated/cc/core/impl/aggregation/protocol/federated_compute_checkpoint_builder.h"

namespace fcp {
namespace client {
namespace engine {

using ::fcp::client::ExampleQueryResult;
using ::fcp::client::engine::PlanResult;
using ::fcp::client::opstats::OpStatsLogger;
using ::fcp::confidentialcompute::PayloadMetadata;
using ::google::internal::federated::plan::DataType;
using ::google::internal::federated::plan::ExampleQuerySpec;
using ::google::internal::federated::plan::ExampleSelector;
using tensorflow_federated::aggregation::CheckpointBuilder;
using ::tensorflow_federated::aggregation::
    FederatedComputeCheckpointBuilderFactory;
using tensorflow_federated::aggregation::Tensor;
using tensorflow_federated::aggregation::TensorShape;

namespace {
// Converts the event time range to CivilSecond for comparison.
absl::CivilSecond ConvertDateTimeToCivilSecond(
    const google::type::DateTime& date_time) {
  return absl::CivilSecond(date_time.year(), date_time.month(), date_time.day(),
                           date_time.hours(), date_time.minutes(),
                           date_time.seconds());
}

// Merges the event time range from the example query results.
EventTimeRange GetEventTimeRange(
    const std::vector<
        std::pair<ExampleQuerySpec::ExampleQuery, ExampleQueryResult>>&
        structured_example_query_results) {
  EventTimeRange merged_event_time_range;
  for (auto const& [example_query, example_query_result] :
       structured_example_query_results) {
    for (auto const& [query_name, event_time_range] :
         example_query_result.stats().event_time_range()) {
      // Expands the merged start time if necessary.
      if (event_time_range.has_start_event_time()) {
        if (merged_event_time_range.has_start_event_time()) {
          // Converts the start times to CivilSecond for comparison.
          absl::CivilSecond merged_start_time = ConvertDateTimeToCivilSecond(
              merged_event_time_range.start_event_time());
          absl::CivilSecond current_start_time =
              ConvertDateTimeToCivilSecond(event_time_range.start_event_time());
          if (current_start_time < merged_start_time) {
            *merged_event_time_range.mutable_start_event_time() =
                event_time_range.start_event_time();
          }
        } else {
          // If the merged event time range does not have a start time, but the
          // current event time range does, then we should set the merged event
          // time range's start time to the current event time range's start
          // time.
          *merged_event_time_range.mutable_start_event_time() =
              event_time_range.start_event_time();
        }
      }

      // Expands the merged end time if necessary.
      if (event_time_range.has_end_event_time()) {
        if (merged_event_time_range.has_end_event_time()) {
          // Converts the end times to CivilSecond for comparison.
          absl::CivilSecond merged_end_time = ConvertDateTimeToCivilSecond(
              merged_event_time_range.end_event_time());
          absl::CivilSecond current_end_time =
              ConvertDateTimeToCivilSecond(event_time_range.end_event_time());
          if (current_end_time > merged_end_time) {
            *merged_event_time_range.mutable_end_event_time() =
                event_time_range.end_event_time();
          }
        } else {
          // If the merged event time range does not have an end time, but the
          // current event time range does, then we should set the merged event
          // time range's end time to the current event time range's end time.
          *merged_event_time_range.mutable_end_event_time() =
              event_time_range.end_event_time();
        }
      }
    }
  }
  return merged_event_time_range;
}

// Converts an ExampleQueryResult to client report wire format tensors.
absl::Status GenerateAggregationTensorsFromExampleQueryResult(
    CheckpointBuilder& checkpoint_builder,
    const ExampleQuerySpec::ExampleQuery& example_query,
    const ExampleQueryResult& example_query_result) {
  for (auto const& [vector_name, vector_tuple] :
       GetOutputVectorSpecs(example_query)) {
    std::string output_name = std::get<0>(vector_tuple);
    ExampleQuerySpec::OutputVectorSpec output_vector_spec =
        std::get<1>(vector_tuple);
    auto it = example_query_result.vector_data().vectors().find(vector_name);
    if (it == example_query_result.vector_data().vectors().end()) {
      return absl::DataLossError(
          "Expected value not found in the example query result");
    }
    const ExampleQueryResult::VectorData::Values values = it->second;
    Tensor tensor;
    if (values.has_int32_values()) {
      FCP_RETURN_IF_ERROR(
          CheckOutputVectorDataType(output_vector_spec, DataType::INT32));
      FCP_ASSIGN_OR_RETURN(
          tensor, ConvertNumericTensor<int32_t>(
                      tensorflow_federated::aggregation::DT_INT32,
                      TensorShape({values.int32_values().value_size()}),
                      values.int32_values().value()));

    } else if (values.has_int64_values()) {
      FCP_RETURN_IF_ERROR(
          CheckOutputVectorDataType(output_vector_spec, DataType::INT64));
      FCP_ASSIGN_OR_RETURN(
          tensor, ConvertNumericTensor<int64_t>(
                      tensorflow_federated::aggregation::DT_INT64,
                      TensorShape({values.int64_values().value_size()}),
                      values.int64_values().value()));
    } else if (values.has_string_values()) {
      FCP_RETURN_IF_ERROR(
          CheckOutputVectorDataType(output_vector_spec, DataType::STRING));
      FCP_ASSIGN_OR_RETURN(
          tensor, ConvertStringTensor(
                      TensorShape({values.string_values().value_size()}),
                      values.string_values().value()));
    } else if (values.has_bool_values()) {
      // TODO: b/296046539 - add support for bool values type
      return absl::UnimplementedError("Bool values currently not supported.");
    } else if (values.has_float_values()) {
      FCP_RETURN_IF_ERROR(
          CheckOutputVectorDataType(output_vector_spec, DataType::FLOAT));
      FCP_ASSIGN_OR_RETURN(
          tensor, ConvertNumericTensor<float>(
                      tensorflow_federated::aggregation::DT_FLOAT,
                      TensorShape({values.float_values().value_size()}),
                      values.float_values().value()));
    } else if (values.has_double_values()) {
      FCP_RETURN_IF_ERROR(
          CheckOutputVectorDataType(output_vector_spec, DataType::DOUBLE));
      FCP_ASSIGN_OR_RETURN(
          tensor, ConvertNumericTensor<double>(
                      tensorflow_federated::aggregation::DT_DOUBLE,
                      TensorShape({values.double_values().value_size()}),
                      values.double_values().value()));
    } else if (values.has_bytes_values()) {
      // TODO: b/296046539 - add support for bytes values type
      return absl::UnimplementedError("Bytes values currently not supported.");
    } else {
      return absl::DataLossError(
          "Unexpected data type in the example query result");
    }
    FCP_RETURN_IF_ERROR(checkpoint_builder.Add(output_name, tensor));
  }
  return absl::OkStatus();
}

// Converts example query results to client report wire format tensors.
// Example query results order must be the same as
// example_query_spec.example_queries.
absl::Status GenerateAggregationTensorsFromStructuredResults(
    CheckpointBuilder& checkpoint_builder,
    const std::vector<
        std::pair<ExampleQuerySpec::ExampleQuery, ExampleQueryResult>>&
        structured_example_query_results,
    const ExampleQuerySpec& example_query_spec) {
  for (auto const& [example_query, example_query_result] :
       structured_example_query_results) {
    FCP_RETURN_IF_ERROR(GenerateAggregationTensorsFromExampleQueryResult(
        checkpoint_builder, example_query, example_query_result));
  }
  return absl::OkStatus();
}

// Converts direct example query results to client report wire format tensors.
absl::Status GenerateAggregationTensorsFromDirectQueryResults(
    CheckpointBuilder& checkpoint_builder,
    const absl::flat_hash_map<std::string, std::vector<std::string>>&
        raw_example_query_results) {
  for (auto const& [vector_name, vector_value] : raw_example_query_results) {
    FCP_ASSIGN_OR_RETURN(Tensor tensor, ConvertStringTensor(&vector_value));
    FCP_RETURN_IF_ERROR(checkpoint_builder.Add(vector_name, tensor));
  }
  return absl::OkStatus();
}

struct InProgressCheckpoint {
  std::unique_ptr<CheckpointBuilder> checkpoint_builder;
  std::optional<PayloadMetadata> payload_metadata;
};

// Creates checkpoint builders for each privacy ID, with a scalar tensor for the
// privacy ID itself. If the checkpoint builder already exists, adds the results
// to the existing checkpoint builder. Expects all ExampleQueryResults to have
// fully populated event time ranges if event time data upload is enabled.
absl::Status CreateOrUpdateCheckpointBuilders(
    const SplitResults& split_results,
    absl::flat_hash_map<std::string, InProgressCheckpoint>&
        in_progress_checkpoints,
    FederatedComputeCheckpointBuilderFactory& checkpoint_builder_factory,
    bool enable_event_time_data_upload) {
  for (const auto& per_privacy_id_result :
       split_results.per_privacy_id_results) {
    if (!in_progress_checkpoints.contains(per_privacy_id_result.privacy_id)) {
      auto checkpoint_builder = checkpoint_builder_factory.Create();
      // Add the privacy ID to the checkpoint builder.
      const std::vector<std::string> privacy_id_vector = {
          per_privacy_id_result.privacy_id};
      FCP_ASSIGN_OR_RETURN(Tensor privacy_id_tensor,
                           ConvertStringTensor(&privacy_id_vector));
      FCP_RETURN_IF_ERROR(checkpoint_builder->Add(
          confidential_compute::kPrivacyIdColumnName, privacy_id_tensor));

      in_progress_checkpoints[per_privacy_id_result.privacy_id] = {
          .checkpoint_builder = std::move(checkpoint_builder)};
    }

    FCP_RETURN_IF_ERROR(GenerateAggregationTensorsFromExampleQueryResult(
        *in_progress_checkpoints[per_privacy_id_result.privacy_id]
             .checkpoint_builder,
        split_results.example_query,
        per_privacy_id_result.example_query_result));

    if (enable_event_time_data_upload) {
      if (!(per_privacy_id_result.example_query_result.stats()
                .cross_query_event_time_range()
                .has_start_event_time() &&
            per_privacy_id_result.example_query_result.stats()
                .cross_query_event_time_range()
                .has_end_event_time())) {
        return absl::InvalidArgumentError(
            "Event time range must be fully populated.");
      }

      if (!in_progress_checkpoints[per_privacy_id_result.privacy_id]
               .payload_metadata.has_value()) {
        PayloadMetadata payload_metadata;
        *payload_metadata.mutable_event_time_range() =
            per_privacy_id_result.example_query_result.stats()
                .cross_query_event_time_range();
        in_progress_checkpoints[per_privacy_id_result.privacy_id]
            .payload_metadata = std::move(payload_metadata);
      } else {
        // Extend the event time range of the in progress checkpoint to include
        // the event time range of the per privacy ID result.
        EventTimeRange current_result_time_range =
            per_privacy_id_result.example_query_result.stats()
                .cross_query_event_time_range();
        EventTimeRange* in_progress_checkpoint_time_range =
            in_progress_checkpoints[per_privacy_id_result.privacy_id]
                .payload_metadata->mutable_event_time_range();

        absl::CivilSecond current_result_start_time =
            ConvertDateTimeToCivilSecond(
                current_result_time_range.start_event_time());
        absl::CivilSecond in_progress_checkpoint_start_time =
            ConvertDateTimeToCivilSecond(
                in_progress_checkpoint_time_range->start_event_time());
        if (current_result_start_time < in_progress_checkpoint_start_time) {
          *in_progress_checkpoint_time_range->mutable_start_event_time() =
              current_result_time_range.start_event_time();
        }

        absl::CivilSecond current_result_end_time =
            ConvertDateTimeToCivilSecond(
                current_result_time_range.end_event_time());
        absl::CivilSecond in_progress_checkpoint_end_time =
            ConvertDateTimeToCivilSecond(
                in_progress_checkpoint_time_range->end_event_time());
        if (current_result_end_time > in_progress_checkpoint_end_time) {
          *in_progress_checkpoint_time_range->mutable_end_event_time() =
              current_result_time_range.end_event_time();
        }
      }
    }
  }
  return absl::OkStatus();
}

// Build the in-progress checkpoints and add them to the plan result.
absl::Status AddCheckpointsToPlanResult(
    const absl::flat_hash_map<std::string, InProgressCheckpoint>&
        in_progress_checkpoints,
    PlanResult& plan_result) {
  for (auto& [privacy_id, in_progress_checkpoint] : in_progress_checkpoints) {
    FederatedComputeCheckpoint checkpoint;
    FCP_ASSIGN_OR_RETURN(checkpoint.payload,
                         in_progress_checkpoint.checkpoint_builder->Build());
    checkpoint.metadata = std::move(in_progress_checkpoint.payload_metadata);
    plan_result.federated_compute_checkpoints.push_back(std::move(checkpoint));
  }
  return absl::OkStatus();
}

}  // anonymous namespace

ExampleQueryPlanEngine::ExampleQueryPlanEngine(
    std::vector<ExampleIteratorFactory*> example_iterator_factories,
    OpStatsLogger* opstats_logger,
    ExampleIteratorQueryRecorder* example_iterator_query_recorder,
    const absl::AnyInvocable<std::unique_ptr<TensorflowRunner>() const>&
        tensorflow_runner_factory)
    : example_iterator_factories_(example_iterator_factories),
      opstats_logger_(opstats_logger),
      example_iterator_query_recorder_(example_iterator_query_recorder),
      tensorflow_runner_factory_(tensorflow_runner_factory) {}

PlanResult ExampleQueryPlanEngine::RunPlan(
    const ExampleQuerySpec& example_query_spec,
    const std::string& output_checkpoint_filename,
    bool use_client_report_wire_format, bool enable_event_time_data_upload,
    std::optional<absl::string_view> source_id, bool uses_confidential_agg,
    bool enable_privacy_id_generation) {
  std::atomic<int> total_example_count = 0;
  std::atomic<int64_t> total_example_size_bytes = 0;
  bool has_event_time_range = false;
  std::vector<std::pair<ExampleQuerySpec::ExampleQuery, ExampleQueryResult>>
      structured_example_query_results;
  absl::flat_hash_map<std::string, std::vector<std::string>>
      direct_example_query_results;
  bool split_results_by_privacy_id = enable_privacy_id_generation &&
                                     example_query_spec.has_privacy_id_config();
  // Checkpoint builders and metadata for each privacy ID. If
  // split_results_by_privacy_id is true, we want results with the same privacy
  // ID in the same checkpoint, regardless of what ExampleQuery they came from.
  absl::flat_hash_map<std::string, InProgressCheckpoint>
      in_progress_checkpoints;
  for (const auto& example_query : example_query_spec.example_queries()) {
    const ExampleSelector& selector = example_query.example_selector();
    ExampleIteratorFactory* example_iterator_factory =
        FindExampleIteratorFactory(selector, example_iterator_factories_);
    if (example_iterator_factory == nullptr) {
      return PlanResult(PlanOutcome::kExampleIteratorError,
                        absl::InternalError(
                            "Could not find suitable ExampleIteratorFactory"));
    }
    absl::StatusOr<std::unique_ptr<ExampleIterator>> example_iterator =
        example_iterator_factory->CreateExampleIterator(selector);
    if (!example_iterator.ok()) {
      return PlanResult(PlanOutcome::kExampleIteratorError,
                        example_iterator.status());
    }
    SingleExampleIteratorQueryRecorder* single_query_recorder = nullptr;
    if (example_iterator_query_recorder_) {
      single_query_recorder =
          example_iterator_query_recorder_->RecordQuery(selector);
    }

    ExampleIteratorStatus example_iterator_status;
    auto dataset_iterator_creator = [&](std::atomic<int32_t>* example_count) {
      return std::make_unique<DatasetIterator>(
          std::move(*example_iterator), opstats_logger_, single_query_recorder,
          example_count, &total_example_size_bytes, &example_iterator_status,
          selector.collection_uri(),
          /*collect_stats=*/example_iterator_factory->ShouldCollectStats());
    };

    if (example_query.direct_output_tensor_name().empty()) {
      // Structured example query
      std::atomic<int> unused_example_count = 0;
      auto dataset_iterator = dataset_iterator_creator(&unused_example_count);

      absl::StatusOr<std::string> example_query_result_str =
          dataset_iterator->GetNext();
      if (!example_query_result_str.ok()) {
        return PlanResult(PlanOutcome::kExampleIteratorError,
                          example_query_result_str.status());
      }

      ExampleQueryResult example_query_result;
      if (!example_query_result.ParseFromString(*example_query_result_str)) {
        return PlanResult(
            PlanOutcome::kExampleIteratorError,
            absl::DataLossError("Unexpected example query result format"));
      }
      // We currently use the number of example query output rows as the
      // 'example count' for the purpose of diagnostic logs. We may want to
      // reconsider this in the future and introduce a proper notion of the
      // total number of examples that were consumed in the example iterator in
      // order to produce those output rows.
      total_example_count += example_query_result.stats().output_rows_count();

      if (split_results_by_privacy_id) {
        if (!source_id.has_value()) {
          // Source ID is set by fl_runner if enable_privacy_id_generation is
          // true, so we should never reach this point.
          return PlanResult(
              PlanOutcome::kExampleIteratorError,
              absl::InvalidArgumentError("Source ID is required for privacy ID "
                                         "generation."));
        }
        if (!uses_confidential_agg) {
          return PlanResult(
              PlanOutcome::kExampleIteratorError,
              absl::InvalidArgumentError("Privacy ID is only supported for "
                                         "confidential aggregation."));
        }
        if (!use_client_report_wire_format) {
          return PlanResult(
              PlanOutcome::kExampleIteratorError,
              absl::InvalidArgumentError(
                  "Privacy ID is only supported for client report wire "
                  "format."));
        }
        absl::StatusOr<SplitResults> split_results = SplitResultsByPrivacyId(
            example_query, example_query_result,
            example_query_spec.privacy_id_config(), *source_id);
        if (!split_results.ok()) {
          return PlanResult(PlanOutcome::kExampleIteratorError,
                            split_results.status());
        }
        // Add the SplitResults to the in-progress checkpoints.
        absl::Status status = CreateOrUpdateCheckpointBuilders(
            *split_results, in_progress_checkpoints,
            federated_compute_checkpoint_builder_factory_,
            enable_event_time_data_upload);
        if (!status.ok()) {
          return PlanResult(PlanOutcome::kExampleIteratorError, status);
        }
      } else {
        // If the example query result stats have event_time_range set, validate
        // that both start and end are set.
        if (enable_event_time_data_upload) {
          for (const auto& [query_name, event_time_range] :
               example_query_result.stats().event_time_range()) {
            if (event_time_range.has_start_event_time() &&
                !event_time_range.has_end_event_time()) {
              return PlanResult(PlanOutcome::kExampleIteratorError,
                                absl::InvalidArgumentError(
                                    "Start event time is specified, but "
                                    "end event time is not for query: " +
                                    query_name));
            }
            if (!event_time_range.has_start_event_time() &&
                event_time_range.has_end_event_time()) {
              return PlanResult(PlanOutcome::kExampleIteratorError,
                                absl::InvalidArgumentError(
                                    "End event time is specified, but "
                                    "start event time is not for query: " +
                                    query_name));
            }
            if (event_time_range.has_start_event_time() &&
                event_time_range.has_end_event_time()) {
              has_event_time_range = true;
            }
          }
        }

        structured_example_query_results.push_back(
            std::make_pair(example_query, std::move(example_query_result)));
      }
    } else {
      // Direct example query
      auto dataset_iterator = dataset_iterator_creator(&total_example_count);
      std::vector<std::string> example_query_results;
      while (true) {
        absl::StatusOr<std::string> example = dataset_iterator->GetNext();
        if (example.status().code() == absl::StatusCode::kOutOfRange) {
          break;
        }
        if (!example.ok()) {
          return PlanResult(PlanOutcome::kExampleIteratorError,
                            example.status());
        }
        example_query_results.push_back(std::move(*example));
      }
      direct_example_query_results[example_query.direct_output_tensor_name()] =
          std::move(example_query_results);
    }
  }
  PlanResult plan_result(PlanOutcome::kSuccess, absl::OkStatus());
  // Note that for the example_size_bytes stat, we use the number reported
  // by the DatasetIterator, since it'll give us the most accurate
  // representation of the amount of data that was actually passed over the
  // ExampleIterator layer. However, the DatasetIterator will only observe a
  // single 'example' for each query it issues, even though that single
  // ExampleQueryResult will likely contain multiple items of data spread
  // across a number of vectors. Instead, we pass the example counts
  // calculated by example store layer directly.
  plan_result.example_stats = {.example_count = total_example_count,
                               .example_size_bytes = total_example_size_bytes};

  absl::Status status;
  if (use_client_report_wire_format) {
    if (split_results_by_privacy_id) {
      // Add all the in-progress checkpoints to the plan result.
      absl::Status status =
          AddCheckpointsToPlanResult(in_progress_checkpoints, plan_result);
      if (!status.ok()) {
        return PlanResult(PlanOutcome::kExampleIteratorError, status);
      }
      return plan_result;
    }
    // Create a checkpoint for ExampleQueryResults that aren't split by
    // privacy ID.
    auto checkpoint_builder =
        federated_compute_checkpoint_builder_factory_.Create();
    // First add all the tensors from the structured example query results.
    status = GenerateAggregationTensorsFromStructuredResults(
        *checkpoint_builder, structured_example_query_results,
        example_query_spec);
    if (!status.ok()) {
      return PlanResult(PlanOutcome::kExampleIteratorError, status);
    }
    // Add all the tensors from the direct example queries.
    status = GenerateAggregationTensorsFromDirectQueryResults(
        *checkpoint_builder, direct_example_query_results);
    if (!status.ok()) {
      return PlanResult(PlanOutcome::kExampleIteratorError, status);
    }

    absl::StatusOr<absl::Cord> checkpoint = checkpoint_builder->Build();
    if (!checkpoint.ok()) {
      return PlanResult(PlanOutcome::kExampleIteratorError,
                        checkpoint.status());
    }

    FederatedComputeCheckpoint result_checkpoint = {.payload =
                                                        std::move(*checkpoint)};

    // If event time data upload is enabled, and the example query results
    // have event time ranges, then we should set the payload metadata in
    // the plan result.
    if (enable_event_time_data_upload && has_event_time_range) {
      PayloadMetadata payload_metadata;
      *payload_metadata.mutable_event_time_range() =
          GetEventTimeRange(structured_example_query_results);
      result_checkpoint.metadata = std::move(payload_metadata);
    }
    plan_result.federated_compute_checkpoints.push_back(
        std::move(result_checkpoint));
  } else {
    // Direct example query results don't support TF v1 checkpoint format. If
    // the direct example query results are not empty, there's a developer error
    // somewhere.
    FCP_CHECK(direct_example_query_results.empty());
    if (tensorflow_runner_factory_) {
      std::unique_ptr<TensorflowRunner> tensorflow_runner =
          tensorflow_runner_factory_();
      status = tensorflow_runner->WriteTFV1Checkpoint(
          output_checkpoint_filename, structured_example_query_results);
    } else {
      status = absl::UnimplementedError(
          "TF v1 checkpoint must be output, but TensorflowRunner is not "
          "registered.");
    }
  }
  if (!status.ok()) {
    return PlanResult(PlanOutcome::kExampleIteratorError, status);
  }

  return plan_result;
}

}  // namespace engine
}  // namespace client
}  // namespace fcp
