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
#ifndef FCP_CLIENT_FEDERATED_PROTOCOL_H_
#define FCP_CLIENT_FEDERATED_PROTOCOL_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/node_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/time/time.h"
#include "fcp/client/engine/engine.pb.h"
#include "fcp/client/stats.h"
#include "fcp/protos/confidentialcompute/payload_metadata.pb.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/protos/federatedcompute/task_assignments.pb.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/protos/population_eligibility_spec.pb.h"

namespace fcp {
namespace client {

// Data type used to encode results of a computation - a TensorFlow
// checkpoint, or SecAgg quantized tensors.
// For non-SecAgg use (simple federated aggregation, or local computation),
// this map should only contain one entry - a TFCheckpoint - and the string
// should be ignored by downstream code.
// For SecAgg use, there should be
// * at most one TFCheckpoint - again, the key should be ignored - and
// * N QuantizedTensors, whose string keys must map to the tensor names
//   provided in the server's CheckinResponse's SideChannelExecutionInfo.
using TFCheckpoint = std::string;
// Data type used to represent Federated Compute wire format checkpoint.
using FCCheckpoint = absl::Cord;
struct QuantizedTensor {
  std::vector<uint64_t> values;
  int32_t bitwidth = 0;
  std::vector<int64_t> dimensions;

  QuantizedTensor() = default;
  // Disallow copy and assign.
  QuantizedTensor(const QuantizedTensor&) = delete;
  QuantizedTensor& operator=(const QuantizedTensor&) = delete;
  // Enable move semantics.
  QuantizedTensor(QuantizedTensor&&) = default;
  QuantizedTensor& operator=(QuantizedTensor&&) = default;
};
// This is equivalent to using ComputationResults =
//    std::map<std::string, std::variant<TFCheckpoint, QuantizedTensor>>;
// except copy construction and assignment are explicitly prohibited and move
// semantics are enforced.
class ComputationResults
    : public absl::node_hash_map<
          std::string,
          std::variant<TFCheckpoint, QuantizedTensor, FCCheckpoint>> {
 public:
  using Base = absl::node_hash_map<
      std::string, std::variant<TFCheckpoint, QuantizedTensor, FCCheckpoint>>;
  using Base::Base;
  using Base::operator=;
  ComputationResults(const ComputationResults&) = delete;
  ComputationResults& operator=(const ComputationResults&) = delete;
  ComputationResults(ComputationResults&&) = default;
  ComputationResults& operator=(ComputationResults&&) = default;
};

// An interface that represents a single Federated Compute protocol session.
//
// An instance of this class represents a single session of client-server
// interaction. Instances are generally stateful, and therefore cannot be
// reused (each session should use a dedicated instance).
//
// The protocol consists of 3 phases, which must occur in the following order:
// 1. A call to `EligibilityEvalCheckin()`.
// 2. A call to `Checkin(...)`, only if the client wasn't rejected by the server
//    in the previous phase.
// 3. A call to `ReportCompleted(...)` or `ReportNotCompleted(...)`, only if the
//    client wasn't rejected in the previous phase.
class FederatedProtocol {
 public:
  virtual ~FederatedProtocol() = default;

  // The unparsed plan and checkpoint payload which make up a computation. The
  // data can be provided as either an std::string or an absl::Cord.
  struct PlanAndCheckpointPayloads {
    std::variant<std::string, absl::Cord> plan;
    std::variant<std::string, absl::Cord> checkpoint;
  };

  // An eligibility task, consisting of task payloads and an execution ID.
  struct EligibilityEvalTask {
    PlanAndCheckpointPayloads payloads;
    std::string execution_id;
    // TODO(team): Replace this with absl::Cord and move the parsing to
    // fl_runner.
    std::optional<google::internal::federated::plan::PopulationEligibilitySpec>
        population_eligibility_spec;
    std::string content_binding;
  };
  // A rejection of a client by the server.
  struct Rejection {};
  // Indicates that the server does not have an eligibility eval task configured
  // for the population.
  struct EligibilityEvalDisabled {
    // TODO(team): Replace this with absl::Cord and move the parsing to
    // fl_runner.
    std::optional<google::internal::federated::plan::PopulationEligibilitySpec>
        population_eligibility_spec;
    std::string content_binding;
  };
  // EligibilityEvalCheckin() returns either
  // 1. an `EligibilityEvalTask` struct holding the payloads for an eligibility
  //    eval task, if the population is configured with such a task. In this
  //    case the caller should execute the task and pass the resulting
  //    `TaskEligibilityInfo` value to the `Checkin(...)` method.
  // 2. an `EligibilityEvalDisabled` struct if the population doesn't have an
  //    eligibility eval task configured. In this case the caller should
  //    continue the protocol by calling the `Checkin(...)` method without
  //    providing a `TaskEligibilityInfo` value.
  // 3. a `Rejection` if the server rejected this device. In this case the
  //    caller should end its protocol interaction.
  using EligibilityEvalCheckinResult =
      std::variant<EligibilityEvalTask, EligibilityEvalDisabled, Rejection>;

  // Checks in with a federated server to receive the population's eligibility
  // eval task. This method is optional and may be called 0 or 1 times. If it is
  // called, then it must be called before any call to `Checkin(...)`.
  //
  // If an eligibility eval task is configured, then the
  // `payload_uris_received_callback` function will be called with a partially
  // populated `EligibilityEvalTask` containing all of the task's info except
  // for the actual payloads (which are yet to be fetched at that point).
  //
  // Returns:
  // - On success, an EligibilityEvalCheckinResult.
  // - On error:
  //   - ABORTED when one of the I/O operations got aborted by the server.
  //   - CANCELLED when one of the I/O operations was interrupted by the client
  //     (possibly due to a positive result from the should_abort callback).
  //   - UNAVAILABLE when server cannot be reached or URI is invalid.
  //   - NOT_FOUND if the server responds with NOT_FOUND, e.g. because the
  //     specified population name is incorrect.
  //   - UNIMPLEMENTED if an unexpected server response is received.
  //   - INTERNAL if the server-provided ClientOnlyPlan cannot be parsed. (See
  //     note in federated_protocol.cc for the reasoning for this.)
  //   - INTERNAL for other unexpected client-side errors.
  //   - any server-provided error code.
  virtual absl::StatusOr<EligibilityEvalCheckinResult> EligibilityEvalCheckin(
      std::function<void(const EligibilityEvalTask&)>
          payload_uris_received_callback) = 0;

  // Report an eligibility eval task error to the federated server.
  // Must only be called once and after a successful call to
  // EligibilityEvalCheckin() which returns an eligibility eval task. This
  // method is only used to report an error happened during the computation of
  // the eligibility eval task. If the eligibility eval computation succeeds,
  // the success will be reported during task assignment.
  // @param status the outcome of the eligibility eval computation.
  virtual void ReportEligibilityEvalError(absl::Status error_status) = 0;

  // SecAgg metadata, e.g. see SecureAggregationProtocolExecutionInfo in
  // federated_api.proto.
  struct SecAggInfo {
    int32_t expected_number_of_clients;
    int32_t minimum_clients_in_server_visible_aggregate;
  };

  // Metadata about the confidential aggregation method with which the task's
  // result will be processed.
  struct ConfidentialAggInfo {
    // The serialized `fcp.confidentialcompute.DataAccessPolicy` proto that will
    // apply to the client's contribution to this task.
    absl::Cord data_access_policy;
    // The serialized `fcp.confidentialcompute.SignedEndorsements` proto that
    // will be used to verify the data access policy above.
    absl::Cord signed_endorsements;
  };

  // A task assignment, consisting of task payloads, a URI template to download
  // federated select task slices with (if the plan uses federated select), a
  // session identifier, and SecAgg-related metadata.
  struct TaskAssignment {
    PlanAndCheckpointPayloads payloads;
    std::string federated_select_uri_template;
    std::string aggregation_session_id;
    // Only populated when the task will be aggregated using the secure
    // aggregation protocol.
    std::optional<SecAggInfo> sec_agg_info;
    // Only populated when the task will be aggregated using the confidential
    // aggregation protocol.
    std::optional<ConfidentialAggInfo> confidential_agg_info;
    std::string task_name;
    // Unique identifier for the task when the task is assigned via multiple
    // task assignment.
    std::string task_identifier;
  };
  // Checkin() returns either
  // 1. a `TaskAssignment` struct if the client was assigned a task to run, or
  // 2. a `Rejection` struct if the server rejected this device.
  using CheckinResult = std::variant<TaskAssignment, Rejection>;

  // Checks in with a federated server. Must only be called once. If the
  // `EligibilityEvalCheckin()` method was previously called, then this method
  // must only be called if the result of that call was not a `Rejection`.
  //
  // If the caller previously called `EligibilityEvalCheckin()` and:
  // - received a payload, then the `TaskEligibilityInfo` value computed by that
  //   payload must be provided via the `task_eligibility_info` parameter.
  // - received an `EligibilityEvalDisabled` result, then the
  //   `task_eligibility_info` parameter should be left empty.
  //
  // If the caller did not previously call `EligibilityEvalCheckin()`, then the
  // `task_eligibility_info` parameter should be left empty.
  //
  // If the client is assigned a task by the server, then the
  // `payload_uris_received_callback` function will be called with a partially
  // populated `TaskAssignment` containing all of the task's info except for the
  // actual payloads (which are yet to be fetched at that point)
  //
  // Returns:
  // - On success, a `CheckinResult`.
  // - On error:
  //   - ABORTED when one of the I/O operations got aborted by the server.
  //   - CANCELLED when one of the I/O operations was interrupted by the client
  //     (possibly due to a positive result from the should_abort callback).
  //   - UNAVAILABLE when server cannot be reached or URI is invalid.
  //   - NOT_FOUND if the server responds with NOT_FOUND, e.g. because the
  //     specified population name is incorrect.
  //   - UNIMPLEMENTED if an unexpected server response is received.
  //   - INTERNAL if the server-provided ClientOnlyPlan cannot be parsed. (See
  //     note in federated_protocol.cc for the reasoning for this.)
  //   - INTERNAL for other unexpected client-side errors.
  //   - any server-provided error code.
  // TODO(team): Replace this reference to protocol-specific
  // TaskEligibilityInfo proto with a protocol-agnostic struct.
  virtual absl::StatusOr<CheckinResult> Checkin(
      const std::optional<
          google::internal::federatedml::v2::TaskEligibilityInfo>&
          task_eligibility_info,
      std::function<void(const TaskAssignment&)> payload_uris_received_callback,
      const std::optional<std::string>& attestation_measurement) = 0;

  // A map of task names to absl::StatusOr<TaskAssignment> returned
  // by PerformMultipleTaskAssignments. Individual
  // absl::StatusOr<TaskAssignment> may be an error status due to failed to
  // fetch the plan resources.
  // Using btree_map to retain the order of the task assignments. It makes tests
  // much easier to write.
  struct MultipleTaskAssignments {
    absl::btree_map<std::string, absl::StatusOr<TaskAssignment>>
        task_assignments;
  };

  // Checks in with a federated server to get multiple task assignments.
  //
  // Must only be called once after the following conditions are met:
  //
  // - the caller previously called `EligibilityEvalCheckin()` and,
  // - received a payload, and the returned EligibilityEvalTask's
  // `PopulationEligibilitySpec` contained at least one task with
  // TASK_ASSIGNMENT_MODE_MULTIPLE, for which the device is eligible.
  //
  // The input parameter for payload_uris_received_callback is the number of
  // TaskAssignment the client received from the server.  The callback uses this
  // number to check whether the uris (plan and checkpoint) for every requested
  // tasks are received, or only uris for a subset of the requested tasks are
  // received.
  //
  // Returns:
  // - On success, a `MultipleTaskAssignments`.
  // - On error:
  //   - ABORTED when one of the I/O operations got aborted by the server.
  //   - CANCELLED when one of the I/O operations was interrupted by the client
  //     (possibly due to a positive result from the should_abort callback).
  //   - UNAVAILABLE when server cannot be reached or URI is invalid.
  //   - NOT_FOUND if the server responds with NOT_FOUND, e.g. because the
  //     specified population name is incorrect.
  //   - UNIMPLEMENTED if an unexpected server response is received.
  //   - INTERNAL for other unexpected client-side errors.
  //   - any server-provided error code.
  virtual absl::StatusOr<MultipleTaskAssignments>
  PerformMultipleTaskAssignments(
      const std::vector<std::string>& task_names,
      const std::function<void(size_t)>& payload_uris_received_callback,
      const std::optional<std::string>& attestation_measurement) = 0;

  // Reports the result of a federated computation to the server. Must only be
  // called once and after a successful call to Checkin().
  // @param results the ComputationResults of a task.
  // @param plan_duration the duration for executing the plan in the plan
  //        engine. Does not include time spent on downloading the plan.
  // @param task_identifier the identifier of the task, this field is only
  //        filled when the task is assigned via multiple task assignment.
  // @param payload_metadata the metadata of the payload, to be included in each
  // uploaded data blob header.
  // Returns:
  // - On success, OK.
  // - On error (e.g. an interruption, network error, or other unexpected
  //   error):
  //   - ABORTED when one of the I/O operations got aborted by the server.
  //   - CANCELLED when one of the I/O operations was interrupted by the client
  //     (possibly due to a positive result from the should_abort callback).
  //   - UNIMPLEMENTED if the server responded with an unexpected response
  //     message.
  //   - INTERNAL for other unexpected client-side errors.
  //   - any server-provided error code.
  virtual absl::Status ReportCompleted(
      ComputationResults results, absl::Duration plan_duration,
      std::optional<std::string> task_identifier,
      std::optional<confidentialcompute::PayloadMetadata> payload_metadata) = 0;

  // Reports the unsuccessful result of a federated computation to the server.
  // Must only be called once and after a successful call to Checkin().
  // @param phase_outcome the outcome of the federated computation.
  // @param plan_duration the duration for executing the plan in the plan
  //        engine. Does not include time spent on downloading the plan.
  // @param task_identifier the identifier of the task, this field is only
  //        filled when the task is assigned via multiple task assignment.
  // Returns:
  // - On success, OK.
  // - On error:
  //   - ABORTED when one of the I/O operations got aborted by the server.
  //   - CANCELLED when one of the I/O operations was interrupted by the client
  //     (possibly due to a positive result from the should_abort callback).
  //   - UNIMPLEMENTED if the server responded with an unexpected response
  //     message, or if the results to report require SecAgg support.
  //   - INTERNAL for other unexpected client-side errors.
  //   - any server-provided error code.
  virtual absl::Status ReportNotCompleted(
      engine::PhaseOutcome phase_outcome, absl::Duration plan_duration,
      std::optional<std::string> task_identifier) = 0;

  // Returns the RetryWindow the caller should use when rescheduling, based on
  // the current protocol phase. The value returned by this method may change
  // after every interaction with the protocol, so callers should call this
  // right before ending their interactions with the FederatedProtocol object to
  // ensure they use the most recent value.
  // TODO(team): Replace this reference to protocol-specific
  // RetryWindow proto with a protocol-agnostic struct (or just a single
  // absl::Duration).
  virtual google::internal::federatedml::v2::RetryWindow
  GetLatestRetryWindow() = 0;

  // Returns the best estimate of the total bytes downloaded and uploaded over
  // the network, plus the best estimate of the duration of wall clock time
  // spent waiting for network requests to finish (but, for example, excluding
  // any idle time spent waiting between issuing polling requests).
  //
  // Note that this estimate may still include time spent simply waiting for a
  // server response, even if no data was being sent or received during that
  // time. E.g. in the case of the legacy gRPC protocol where the single checkin
  // request blocks until a task is assigned to the client.
  //
  // If possible, this estimate should also include time spent
  // compressing/decompressing payloads before writing them to or after reading
  // them from the network.
  virtual NetworkStats GetNetworkStats() = 0;

 protected:
  // A list of states representing the sequence of calls we expect to receive
  // via this interface, as well as their possible outcomes. Implementations of
  // this class are likely to share these coarse-grained states, and use them to
  // determine which values to return from `GetLatestRetryWindow()`.
  enum class ObjectState {
    // The initial object state.
    kInitialized,
    // EligibilityEvalCheckin() was called but it failed with a 'transient'
    // error (e.g. an UNAVAILABLE network error, although the set of transient
    // errors is flag-defined).
    kEligibilityEvalCheckinFailed,
    // EligibilityEvalCheckin() was called but it failed with a 'permanent'
    // error (e.g. a NOT_FOUND network error, although the set of permanent
    // errors is flag-defined).
    kEligibilityEvalCheckinFailedPermanentError,
    // EligibilityEvalCheckin() was called, and the server rejected the client.
    kEligibilityEvalCheckinRejected,
    // EligibilityEvalCheckin() was called, and the server did not return an
    // eligibility eval payload.
    kEligibilityEvalDisabled,
    // EligibilityEvalCheckin() was called, and the server did return an
    // eligibility eval payload, which must then be run to produce a
    // TaskEligibilityInfo value.
    kEligibilityEvalEnabled,
    // Checkin(...) was called but it failed with a 'transient' error.
    kCheckinFailed,
    // Checkin(...) was called but it failed with a 'permanent' error.
    kCheckinFailedPermanentError,
    // Checkin(...) was called, and the server rejected the client.
    kCheckinRejected,
    // Checkin(...) was called, and the server accepted the client and returned
    // a payload, which must then be run to produce a report.
    kCheckinAccepted,
    // PerformMultipleTaskAssignments(...) was called but it failed with a
    // 'transient' error, without receiving a single task assignment. If some
    // task assignments were successfully received, but some others failed (e.g.
    // because their resources failed to be downloaded), then this state won't
    // be used.
    kMultipleTaskAssignmentsFailed,
    // PerformMultipleTaskAssignments(...) was called but it failed with a
    // 'permanent' error.
    kMultipleTaskAssignmentsFailedPermanentError,
    // PerformMultipleTaskAssignments(...) was called but an empty list of tasks
    // is returned by the server.
    kMultipleTaskAssignmentsNoAvailableTask,
    // PerformMultipleTaskAssignments(...) was called, and the server accepted
    // the client and returned one or more payload, which must then be run to
    // produce a report.
    kMultipleTaskAssignmentsAccepted,
    // Report(...) was called.
    kReportCalled,
    // Report(...) was called and it resulted in a 'permanent' error.
    //
    // Note: there is no kReportFailed (corresponding to 'transient' errors,
    // like the other phases have), because by the time the report phase is
    // reached, a set of RetryWindows is guaranteed to have been received from
    // the server.
    kReportFailedPermanentError,
    // Report(...) was called for multiple tasks, and only a subset of the tasks
    // succeed.
    kReportMultipleTaskPartialError,
  };

  std::string ObjectStateToString(ObjectState state) {
    switch (state) {
      case ObjectState::kInitialized:
        return "kInitialized";
      case ObjectState::kEligibilityEvalCheckinFailed:
        return "kEligibilityEvalCheckinFailed";
      case ObjectState::kEligibilityEvalCheckinFailedPermanentError:
        return "kEligibilityEvalCheckinFailedPermanentError";
      case ObjectState::kEligibilityEvalCheckinRejected:
        return "kEligibilityEvalCheckinRejected";
      case ObjectState::kEligibilityEvalDisabled:
        return "kEligibilityEvalDisabled";
      case ObjectState::kEligibilityEvalEnabled:
        return "kEligibilityEvalEnabled";
      case ObjectState::kCheckinFailed:
        return "kCheckinFailed";
      case ObjectState::kCheckinFailedPermanentError:
        return "kCheckinFailedPermanentError";
      case ObjectState::kCheckinRejected:
        return "kCheckinRejected";
      case ObjectState::kCheckinAccepted:
        return "kCheckinAccepted";
      case ObjectState::kMultipleTaskAssignmentsFailed:
        return "kMultipleTaskAssignmentsFailed";
      case ObjectState::kMultipleTaskAssignmentsFailedPermanentError:
        return "kMultipleTaskAssignmentsFailedPermanentError";
      case ObjectState::kMultipleTaskAssignmentsNoAvailableTask:
        return "kMultipleTaskAssignmentsNoAvailableTask";
      case ObjectState::kMultipleTaskAssignmentsAccepted:
        return "kMultipleTaskAssignmentsAccepted";
      case ObjectState::kReportCalled:
        return "kReportCalled";
      case ObjectState::kReportFailedPermanentError:
        return "kReportFailedPermanentError";
      case ObjectState::kReportMultipleTaskPartialError:
        return "kReportMultipleTaskPartialError";
    }
    return "kUnexpectedValueForObjectState";
  }
};

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_FEDERATED_PROTOCOL_H_
