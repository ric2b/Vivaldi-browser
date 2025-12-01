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
#ifndef FCP_CLIENT_FLAGS_H_
#define FCP_CLIENT_FLAGS_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"

namespace fcp {
namespace client {

// A class for changing runtime behavior with "flags" - typically, server
// provided values.
class Flags {
 public:
  virtual ~Flags() = default;

  // The maximum number of times to retry an HTTP request. If 0, retrying is
  // disabled.
  virtual int32_t http_retry_max_attempts() const { return 3; }

  // The delay (in milliseconds) to wait before retrying an HTTP request.
  virtual int32_t http_retry_delay_ms() const { return 5000; }

  // The period of time in milliseconds between device condition checks. This is
  // used during potentially long blocking calls such as TensorFlow or network
  // I/O, as well as for throttling regular condition checks during plan
  // execution (e.g. before fetching a new example).
  virtual int64_t condition_polling_period_millis() const = 0;

  // The period of time in milliseconds allowed for TensorFlow execution to
  // finish after it's been interrupted.
  virtual int64_t tf_execution_teardown_grace_period_millis() const = 0;

  // The period of time in milliseconds allowed for TensorFlow execution to
  // finish after the grace period. This allows us to decide if we want long
  // running native execution to be forcibly resolved or continue indefinitely.
  virtual int64_t tf_execution_teardown_extended_period_millis() const = 0;

  // Whether to log the error message strings from TensorFlow exceptions.
  virtual bool log_tensorflow_error_messages() const = 0;

  // The number of days for data to live in the OpStatsDb without update.
  virtual int64_t opstats_ttl_days() const { return 30; }

  // The maximum size of the data stored by OpStatsDb.
  virtual int64_t opstats_db_size_limit_bytes() const {
    return 1 * 1024 * 1024;
  }

  // The retry delay to use when encountering a transient error during a
  // training run before having received a RetryWindow from the server.
  virtual int64_t federated_training_transient_errors_retry_delay_secs() const {
    // 15 minutes
    return 15 * 60;
  }

  // The amount of jitter to apply when using the
  // `federated_training_transient_errors_retry_delay_secs` flag. Must be a
  // value between 0 and 1. E.g. a value of 0.2 means that retry delays will
  // fall within [0.8 * target period, 1.2 * target period).
  virtual float federated_training_transient_errors_retry_delay_jitter_percent()
      const {
    return 0.2;
  }

  // The retry delay to use when encountering a permanent error during a
  // training run (regardless of whether the client already received a
  // RetryWindow from the server).
  virtual int64_t federated_training_permanent_errors_retry_delay_secs() const {
    // 4 hours
    return 4 * 60 * 60;
  }

  // The amount of jitter to apply when using the
  // `federated_training_permanent_errors_retry_delay_secs` flag. Must be a
  // value between 0 and 1. E.g. a value of 0.2 means that retry delays will
  // fall within [0.8 * target period, 1.2 * target period).
  virtual float federated_training_permanent_errors_retry_delay_jitter_percent()
      const {
    return 0.2;
  }

  // The list of error codes that should be considered 'permanent'.
  virtual std::vector<int32_t> federated_training_permanent_error_codes()
      const {
    return {
        // The server returns NOT_FOUND if the client checks in with an unknown
        // population name. While this can be resolved without any client
        // changes by creating the population server-side, it is nevertheless
        // wise to treat this as a 'permanent' error for which a longer
        // RetryPeriod is used, because such temporary mismatches in
        // client/server configuration are fairly common and otherwise cause
        // clients to check in unnecessarily frequently.
        static_cast<int32_t>(absl::StatusCode::kNotFound),
        // INVALID_ARGUMENT generally indicates a client-side issue (e.g. a bug
        // in the client's protocol implementation), which is unlikely to be
        // resolved by merely retrying the request.
        static_cast<int32_t>(absl::StatusCode::kInvalidArgument),
        // UNIMPLEMENTED similarly could indicate a client-side issue, or a
        // temporary server issue (e.g. a bug/missing feature implementation in
        // the server). Either way, it is also unlikely to be resolved by merely
        // retrying the request soon.
        static_cast<int32_t>(absl::StatusCode::kUnimplemented)};
  }

  // Whether use TFLite for training. True by default, but can be turned off to
  // force the legacy TFMobile code path to be used instead.
  virtual bool use_tflite_training() const { return true; }

  // When true, TFLite interpreter will use dynamic memory allocation, and
  // release the memory for tensors that are no longer needed.
  virtual bool ensure_dynamic_tensors_are_released() const { return true; }

  // When the value is above zero, any tensor size (bytes) above the threshold
  // will be considered as a large tensor, and dynamic allocation is applied on
  // them.
  virtual int32_t large_tensor_threshold_for_dynamic_allocation() const {
    return 1000;
  }

  // When true, the TFLite runtime graph-reordering optimization that clusters
  // delegate nodes together is disabled.
  virtual bool disable_tflite_delegate_clustering() const { return false; }

  // When true, we'll use  use TFLite's BuiltinOpResolver (as opposed to
  // BuiltinOpResolverWithoutDefaultDelegates).
  virtual bool tflite_use_builtin_op_resolver_with_default_delegates() const {
    return false;
  }

  // When true, http request body won't be compressed.
  virtual bool disable_http_request_body_compression() const { return false; }

  // When true, the client computes the task identity to pass in
  // SelectorContext.
  virtual bool enable_computation_id() const { return false; }

  // The waiting period for issuing cancellation requests before checking
  // whether the client should be interrupted.
  virtual int32_t waiting_period_sec_for_cancellation() const { return 10; }

  // If true, the client supports the Federated Select feature. If not
  // then any Federated Select-specific example query will fail with an error
  virtual bool enable_federated_select() const { return false; }

  // The max size in bytes of resources that the ResourceCache is allowed to
  // store. If greater than 0, the client will attempt to cache resources that
  // it downloads via HTTP URIs. If this value is reduced from some previous
  // greater value, the cache dir will be reduced appropriately the next time it
  // is initialized at the start of the next run.
  virtual int64_t max_resource_cache_size_bytes() const { return 0; }

  // If true, an error during the initialization of the resource cache will
  // publish a fatal initialization error instead of a nonfatal initialization
  // error and halt execution.
  virtual bool resource_cache_initialization_error_is_fatal() const {
    return false;
  }

  // The number of threads that TFLite interpreter will use.
  virtual int32_t num_threads_for_tflite() const { return 2; }

  // If true, enables new client report wire format for lightweight client.
  virtual bool enable_lightweight_client_report_wire_format() const {
    return false;
  }

  // If true, confidential aggregation is advertised & can be used for uploads.
  virtual bool enable_confidential_aggregation() const { return false; }

  // If true, the minimum separation policy will first check if the OpStatsDb's
  // earliest trustworthy time is earlier than the required minimum
  // trustworthiness period.
  virtual bool check_trustworthiness_for_min_sep_policy() const {
    return false;
  }

  // If true, direct data upload tasks are supported.
  virtual bool enable_direct_data_upload_task() const { return false; }

  // If true, adds support for relative URI prefixes.
  virtual bool enable_relative_uri_prefix() const { return false; }

  // If true, the minimum separation policy index will be logged to PhaseStats.
  virtual bool log_min_sep_index_to_phase_stats() const { return false; }

  // If true, the event time data upload is enabled.
  virtual bool enable_event_time_data_upload() const { return false; }

  // If true, the blob header will be added to the HTTP headers for confidential
  // compute uploads.
  virtual bool enable_blob_header_in_http_headers() const { return false; }

  // If true, the attestation measurement will be moved to just before the
  // startTaskAssignment call.
  virtual bool move_device_attestation_to_start_task_assignment() const {
    return false;
  }
  // If true, a rotating privacy ID will be generated for each upload, enabling
  // the application of stronger user-level DP guarantees across uploads from
  // the same device. This is only used for confidential aggregation tasks, and
  // the privacy ID will only be stored in the encrypted payload.
  virtual bool enable_privacy_id_generation() const { return false; }
};
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_FLAGS_H_
