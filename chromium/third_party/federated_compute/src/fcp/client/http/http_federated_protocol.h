/*
 * Copyright 2022 Google LLC
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
#ifndef FCP_CLIENT_HTTP_HTTP_FEDERATED_PROTOCOL_H_
#define FCP_CLIENT_HTTP_HTTP_FEDERATED_PROTOCOL_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fcp/base/clock.h"
#include "fcp/base/wall_clock_stopwatch.h"
#include "fcp/client/attestation/attestation_verifier.h"
#include "fcp/client/cache/resource_cache.h"
#include "fcp/client/engine/engine.pb.h"
#include "fcp/client/federated_protocol.h"
#include "fcp/client/fl_runner.pb.h"
#include "fcp/client/flags.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/in_memory_request_response.h"
#include "fcp/client/http/protocol_request_helper.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/secagg_event_publisher.h"
#include "fcp/client/secagg_runner.h"
#include "fcp/client/selector_context.pb.h"
#include "fcp/client/stats.h"
#include "fcp/protos/confidentialcompute/payload_metadata.pb.h"
#include "fcp/protos/confidentialcompute/signed_endorsements.pb.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/protos/federatedcompute/common.pb.h"
#include "fcp/protos/federatedcompute/confidential_aggregations.pb.h"
#include "fcp/protos/federatedcompute/eligibility_eval_tasks.pb.h"
#include "fcp/protos/federatedcompute/secure_aggregations.pb.h"
#include "fcp/protos/federatedcompute/task_assignments.pb.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/protos/population_eligibility_spec.pb.h"

namespace fcp {
namespace client {
namespace http {

inline constexpr absl::string_view kTaskIdentifierPrefix = "task_";

// Implements a single session of the HTTP-based Federated Compute protocol.
class HttpFederatedProtocol : public fcp::client::FederatedProtocol {
 public:
  HttpFederatedProtocol(
      Clock* clock, LogManager* log_manager, const Flags* flags,
      HttpClient* http_client,
      std::unique_ptr<SecAggRunnerFactory> secagg_runner_factory,
      SecAggEventPublisher* secagg_event_publisher,
      cache::ResourceCache* resource_cache,
      std::unique_ptr<attestation::AttestationVerifier> attestation_verifier,
      absl::string_view entry_point_uri, absl::string_view api_key,
      absl::string_view population_name, absl::string_view retry_token,
      absl::string_view client_version,
      absl::string_view client_attestation_measurement,
      std::function<bool()> should_abort, absl::BitGen bit_gen,
      const InterruptibleRunner::TimingConfig& timing_config);

  ~HttpFederatedProtocol() override = default;

  absl::StatusOr<fcp::client::FederatedProtocol::EligibilityEvalCheckinResult>
  EligibilityEvalCheckin(std::function<void(const EligibilityEvalTask&)>
                             payload_uris_received_callback) override;

  void ReportEligibilityEvalError(absl::Status error_status) override;

  absl::StatusOr<fcp::client::FederatedProtocol::CheckinResult> Checkin(
      const std::optional<
          google::internal::federatedml::v2::TaskEligibilityInfo>&
          task_eligibility_info,
      std::function<void(const TaskAssignment&)> payload_uris_received_callback,
      const std::optional<std::string>& attestation_measurement) override;

  absl::StatusOr<MultipleTaskAssignments> PerformMultipleTaskAssignments(
      const std::vector<std::string>& task_names,
      const std::function<void(size_t)>& payload_uris_received_callback,
      const std::optional<std::string>& attestation_measurement) override;

  absl::Status ReportCompleted(
      ComputationResults results, absl::Duration plan_duration,
      std::optional<std::string> task_identifier,
      std::optional<confidentialcompute::PayloadMetadata> payload_metadata)
      override;

  absl::Status ReportNotCompleted(
      engine::PhaseOutcome phase_outcome, absl::Duration plan_duration,
      std::optional<std::string> task_identifier) override;

  google::internal::federatedml::v2::RetryWindow GetLatestRetryWindow()
      override;

  NetworkStats GetNetworkStats() override;

 private:
  enum class AggregationType {
    // Unknown aggregation type (in cases where the field simply isn't populated
    // yet).
    // TODO: b/307312707 -  Remove this value once the
    // enable_confidential_aggregation() flag is removed.
    kUnknown,
    kSimpleAggregation,
    kSecureAggregation,
    kConfidentialAggregation,
  };
  // Information for a given task.
  struct PerTaskInfo {
    std::unique_ptr<ProtocolRequestCreator> aggregation_request_creator;
    std::unique_ptr<ProtocolRequestCreator> data_upload_request_creator;
    std::string session_id;
    // The identifier of the aggregation session we are participating in.
    std::string aggregation_session_id;
    // The token authorizing the client to participate in an aggregation
    // session.
    std::string aggregation_authorization_token;
    // The name identifying the task that was assigned.
    std::string task_name;
    // Unique identifier for the client's participation in an aggregation
    // session.
    std::string aggregation_client_token;
    // Resource name for the result data to upload.
    std::string aggregation_resource_name;
    // The type of aggregation to use when reporting results for this task.
    // Only set when the enable_confidential_aggregation() flag is enabled. If
    // the flag is disabled then this is unconditionally set to `kUnknown`.
    AggregationType aggregation_type;
    // The serialized data access policy that will be used to govern access to
    // to the data uploaded via the ConfidentialAggregations protocol. Only set
    // when aggregation_type == kConfidentialAggregation.
    std::optional<absl::Cord> confidential_data_access_policy;
    // The signed endorsements that will be used to attest the data access
    // policy above. Only set when aggregation_type == kConfidentialAggregation,
    // and the task uses SignedEndorsements.
    std::optional<absl::Cord> signed_endorsements;
    // Each task's state is tracked individually starting from the end of
    // check-in or multiple task assignments. The states from all of the tasks
    // will be used collectively to determine which retry window to use.
    ObjectState state = ObjectState::kInitialized;
  };

  // Helper function to perform an eligibility eval task request and get its
  // response.
  absl::StatusOr<InMemoryHttpResponse> PerformEligibilityEvalTaskRequest();

  // Helper function for handling an eligibility eval task response (incl.
  // fetching any resources, if necessary).
  absl::StatusOr<fcp::client::FederatedProtocol::EligibilityEvalCheckinResult>
  HandleEligibilityEvalTaskResponse(
      absl::StatusOr<InMemoryHttpResponse> http_response,
      std::function<void(const EligibilityEvalTask&)>
          payload_uris_received_callback);

  absl::StatusOr<std::unique_ptr<HttpRequest>>
  CreateReportEligibilityEvalTaskResultRequest(absl::Status status);

  // Helper function to perform an ReportEligibilityEvalResult request.
  absl::Status ReportEligibilityEvalErrorInternal(absl::Status error_status);

  // Helper function to perform a task assignment request and get its response.
  absl::StatusOr<InMemoryHttpResponse>
  PerformTaskAssignmentAndReportEligibilityEvalResultRequests(
      const std::optional<
          ::google::internal::federatedml::v2::TaskEligibilityInfo>&
          task_eligibility_info,
      std::optional<std::string> attestation_measurement);

  // Helper function for handling the 'outer' task assignment response, which
  // consists of an `Operation` which may or may not need to be polled before a
  // final 'inner' response is available.
  absl::StatusOr<::fcp::client::FederatedProtocol::CheckinResult>
  HandleTaskAssignmentOperationResponse(
      absl::StatusOr<InMemoryHttpResponse> http_response,
      std::function<void(const TaskAssignment&)>
          payload_uris_received_callback);

  // Helper function for handling an 'inner' task assignment response (i.e.
  // after the outer `Operation` has concluded). This includes fetching any
  // resources, if necessary.
  absl::StatusOr<::fcp::client::FederatedProtocol::CheckinResult>
  HandleTaskAssignmentInnerResponse(
      const ::google::protobuf::Any& operation_response,
      std::function<void(const TaskAssignment&)>
          payload_uris_received_callback);

  // Helper function to perform a multiple task assignments request and get its
  // response.
  absl::StatusOr<InMemoryHttpResponse>
  PerformMultipleTaskAssignmentsAndReportEligibilityEvalResult(
      const std::vector<std::string>& task_names,
      std::optional<std::string> attestation_measurement);

  absl::StatusOr<FederatedProtocol::MultipleTaskAssignments>
  HandleMultipleTaskAssignmentsInnerResponse(
      absl::StatusOr<InMemoryHttpResponse> http_response,
      const std::function<void(size_t)>& payload_uris_received_callback);

  // Helper function for reporting result via simple or confidential
  // aggregation.
  absl::Status ReportViaSimpleOrConfidentialAggregation(
      ComputationResults results, absl::Duration plan_duration,
      PerTaskInfo& task_info,
      std::optional<confidentialcompute::PayloadMetadata> payload_metadata);
  // Helper function to perform a StartDataUploadRequest and a ReportTaskResult
  // request concurrently.
  // This method will only return the response from the StartDataUploadRequest.
  absl::StatusOr<InMemoryHttpResponse>
  PerformStartDataUploadRequestAndReportTaskResult(absl::Duration plan_duration,
                                                   PerTaskInfo& task_info);

  // Helper function for handling a `longrunning.Operation` returned by a
  // StartDataAggregationUpload request.
  //
  // Returns an OK status when the response is successfully handled. If the
  // confidential aggregation protocol is being used then the return value will
  // also contain the ConfidentialEncryptionConfig proto describing the
  // parameters used to encrypt the payload.
  absl::StatusOr<std::optional<
      ::google::internal::federatedcompute::v1::ConfidentialEncryptionConfig>>
  HandleStartDataAggregationUploadOperationResponse(
      absl::StatusOr<InMemoryHttpResponse> http_response,
      PerTaskInfo& task_info);

  // Validates a given ConfidentialEncryptionConfig and returns the public key
  // to encrypt the payload with, if the config validation was successful.
  absl::StatusOr<attestation::AttestationVerifier::VerificationResult>
  ValidateConfidentialEncryptionConfig(
      PerTaskInfo& task_info,
      const ::google::internal::federatedcompute::v1::
          ConfidentialEncryptionConfig& encryption_config);

  // Encrypts the given payload using the given public key, and serializes the
  // encrypted payload in a self-describing format suitable for upload to the
  // server.
  absl::StatusOr<std::string> EncryptPayloadForConfidentialAggregation(
      PerTaskInfo& task_info, const std::string& serialized_public_key,
      std::string inner_payload, const std::string& serialized_blob_header);

  // Helper function to perform data upload using the ByteStream protocol, used
  // during simple or confidential aggregation.
  absl::Status UploadDataViaByteStreamProtocol(
      std::string tf_checkpoint, PerTaskInfo& task_info,
      std::optional<absl::string_view> serialized_blob_header);

  // Helper function to perform a SubmitAggregationResult request.
  absl::Status SubmitAggregationResult(PerTaskInfo& task_info);

  // Helper function to perform an AbortAggregation request.
  // We only provide the server with a simplified error message.
  // This function will log a diag code if the abort request failed to be
  // delivered to the server.
  void AbortAggregation(absl::Status original_error_status,
                        absl::string_view error_message_for_server,
                        PerTaskInfo& task_info);
  // The inner implementation that `AbortAggregation` wraps. Having
  // `AbortAggregation` wrap this function makes it easier to ensure we log the
  // diag code for all types of error we may encounter while issuing the abort.
  absl::Status AbortAggregationInner(absl::Status original_error_status,
                                     absl::string_view error_message_for_server,
                                     PerTaskInfo& task_info);

  // Helper function for reporting via secure aggregation.
  absl::Status ReportViaSecureAggregation(ComputationResults results,
                                          absl::Duration plan_duration,
                                          PerTaskInfo& task_info);

  // Helper function to perform a StartSecureAggregationRequest and a
  // ReportTaskResultRequest.
  absl::StatusOr<
      google::internal::federatedcompute::v1::StartSecureAggregationResponse>
  StartSecureAggregationAndReportTaskResult(absl::Duration plan_duration,
                                            PerTaskInfo& task_info);

  // Describes a set of resources that may need to be fetched for a given task.
  // These resources are specified to us after a task is assigned, and should
  // preferably all be downloaded concurrently since the task assignment cannot
  // be returned to the caller until these resources have been fetched.
  struct TaskResources {
    const ::google::internal::federatedcompute::v1::Resource& plan;
    const ::google::internal::federatedcompute::v1::Resource& checkpoint;
    // While all tasks can have a plan and checkpoint Resource, only tasks using
    // the confidential aggregation method have a confidential data access
    // policy (for all other tasks this Resource proto will simply be empty,
    // meaning nothing will be fetched and an empty Cord will be returned).
    const ::google::internal::federatedcompute::v1::Resource&
        confidential_data_access_policy;
    // While all tasks can have a plan and checkpoint Resource, only tasks using
    // the confidential aggregation method may have a signed endorsements (for
    // all other tasks this Resource proto will simply be empty, meaning nothing
    // will be fetched and an empty Cord will be returned).
    const ::google::internal::federatedcompute::v1::Resource&
        signed_endorsements;
  };

  // Represents fetched task resources, separating plan-and-checkpoint from the
  // confidential data access policy, since the latter is only conditionally
  // available.
  struct FetchedTaskResources {
    PlanAndCheckpointPayloads plan_and_checkpoint_payloads;
    // The serialized `fcp.confidentialcompute.DataAccessPolicy` proto, if the
    // task has one, or an empty Cord if the task did not have one.
    absl::Cord confidential_data_access_policy;
    // The serialized `fcp.confidentialcompute.SignedEndorsements` proto, if the
    // task has one, or an empty Cord if the task did not have one.
    absl::Cord signed_endorsements;
  };

  // Helper function for fetching the checkpoint/plan resources for a list of
  // eligibility eval tasks or regular tasks.
  //
  // Returns an error if the fetch operation failed as a whole. Otherwise
  // returns a list of fetch results for each of the specified TaskResources
  // structs, in the same order those structs were passed in. If any of the
  // resources for a specified TaskResources struct failed to be fetched, then
  // an error will be returned in the output vector.
  absl::StatusOr<std::vector<absl::StatusOr<FetchedTaskResources>>>
  FetchTaskResources(std::vector<TaskResources> task_resources_list);

  // Helper function for turning a set of HTTP responses into a
  // `FetchedTaskResources` proto.
  static absl::StatusOr<FetchedTaskResources> CreateFetchedTaskResources(
      absl::StatusOr<InMemoryHttpResponse>& plan_data_response,
      absl::StatusOr<InMemoryHttpResponse>& checkpoint_data_response,
      absl::StatusOr<InMemoryHttpResponse>&
          confidential_data_access_policy_response,
      absl::StatusOr<InMemoryHttpResponse>& signed_endorsements_response);

  // Helper function for fetching the Resources used by the protocol
  // implementation itself, like PopulationEligibilitySpec or
  // ConfidentialEncryptionConfig.
  template <typename T>
  absl::StatusOr<T> FetchProtoResource(
      const ::google::internal::federatedcompute::v1::Resource& resource,
      absl::string_view readable_name);

  // Helper that moves to the given object state if the given status represents
  // a permanent error.
  void UpdateObjectStateIfPermanentError(
      absl::Status status, ObjectState permanent_error_object_state);

  ObjectState GetTheLatestStateFromAllTasks();
  TaskAssignment CreateTaskAssignment(
      const ::google::internal::federatedcompute::v1::TaskAssignment&
          task_assignment,
      std::optional<int32_t> task_index);
  absl::StatusOr<PerTaskInfo> CreatePerTaskInfoFromTaskAssignment(
      const ::google::internal::federatedcompute::v1::TaskAssignment&
          task_assignment,
      ObjectState state);

  absl::Status GetNextTargetUriPrefixAndMaybeUpdateMostRecentForwardingPrefix(
      std::string most_recent_forwarding_prefix,
      ::google::internal::federatedcompute::v1::ForwardingInfo*
          next_target_uri_inf,
      bool should_update);

  // This ObjectState tracks states until the end of check-in or multiple task
  // assignments.  Once a task is assigned, the state is tracked inside the
  // task_info_map_ for multiple task assignments or default_task_info_ for
  // single task check-in.
  ObjectState object_state_;
  Clock& clock_;
  LogManager* log_manager_;
  const Flags* const flags_;
  HttpClient* const http_client_;
  std::unique_ptr<SecAggRunnerFactory> secagg_runner_factory_;
  SecAggEventPublisher* secagg_event_publisher_;
  // `nullptr` if the feature is disabled.
  cache::ResourceCache* resource_cache_;
  // A verifier which can be used to verify a ConfidentialAggregations service's
  // attestation evidence.
  std::unique_ptr<attestation::AttestationVerifier> attestation_verifier_;

  std::unique_ptr<InterruptibleRunner> interruptible_runner_;
  std::unique_ptr<ProtocolRequestCreator> eligibility_eval_request_creator_;
  std::unique_ptr<ProtocolRequestCreator> task_assignment_request_creator_;
  std::unique_ptr<WallClockStopwatch> network_stopwatch_ =
      WallClockStopwatch::Create();
  ProtocolRequestHelper protocol_request_helper_;
  const std::string api_key_;
  const std::string population_name_;
  const std::string retry_token_;
  const std::string client_version_;
  // A measurement with which the client's integrity can be attested to the
  // server.
  const std::string client_attestation_measurement_;
  std::string most_recent_forwarding_prefix_;
  std::function<bool()> should_abort_;
  absl::BitGen bit_gen_;
  const InterruptibleRunner::TimingConfig timing_config_;
  // The graceful waiting period for cancellation requests before checking
  // whether the client should be interrupted.
  const absl::Duration waiting_period_for_cancellation_;
  // The set of canonical error codes that should be treated as 'permanent'
  // errors.
  absl::flat_hash_set<int32_t> federated_training_permanent_error_codes_;
  int64_t bytes_downloaded_ = 0;
  int64_t bytes_uploaded_ = 0;
  // Represents 2 absolute retry timestamps to use when the device is rejected
  // or accepted. The retry timestamps will have been generated based on the
  // retry windows specified in the server's EligibilityEvalTaskResponse message
  // and the time at which that message was received.
  struct RetryTimes {
    absl::Time retry_time_if_rejected;
    absl::Time retry_time_if_accepted;
  };
  // Represents the information received via the EligibilityEvalTaskResponse
  // message. This field will have an absent value until that message has been
  // received.
  std::optional<RetryTimes> retry_times_;
  std::string pre_task_assignment_session_id_;

  // A map of task identifier to per-task information.
  // Only tasks from the multiple task assignments will be tracked in this map.
  absl::flat_hash_map<std::string, PerTaskInfo> task_info_map_;
  // The task received from the regular check-in will be tracked here.
  PerTaskInfo default_task_info_;

  // Set this field to true if an eligibility eval task was received from the
  // server in the EligibilityEvalTaskResponse.
  bool eligibility_eval_enabled_ = false;
  // Set this field to true if ReportEligibilityEvalResult has been called.
  bool report_eligibility_eval_result_called_ = false;
  // Set this field to true if multiple task assignments has been called.
  bool multiple_task_assignments_called_ = false;
};

}  // namespace http
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_HTTP_HTTP_FEDERATED_PROTOCOL_H_
