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
#ifndef FCP_CLIENT_SIMPLE_TASK_ENVIRONMENT_H_
#define FCP_CLIENT_SIMPLE_TASK_ENVIRONMENT_H_

#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "fcp/client/attestation/attestation_verifier.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/selector_context.pb.h"
#include "fcp/client/task_result_info.pb.h"
#include "fcp/protos/confidentialcompute/access_policy_endorsement_options.pb.h"
#include "fcp/protos/plan.pb.h"

namespace fcp {
namespace client {

using ::fcp::confidentialcompute::AccessPolicyEndorsementOptions;

// An interface used by the plan engine to query for serialized examples. Not
// required to be thread-safe.
class ExampleIterator {
 public:
  virtual ~ExampleIterator() = default;

  // Returns a serialized example as std::string on success; or, on error:
  //  - CANCELLED if the call got interrupted.
  //  - INVALID_ARGUMENT if some other error occurred, e.g. I/O.
  //  - OUT_OF_RANGE if the end of the iterator was reached.
  virtual absl::StatusOr<std::string> Next() = 0;

  // Close the iterator to release associated resources.
  virtual void Close() = 0;
};

// A simplified task environment that acts as a standalone task environment for
// TFF-based plans and a delegated task environment for earlier plan types.
class SimpleTaskEnvironment {
 public:
  virtual ~SimpleTaskEnvironment() = default;

  // Returns the path of the directory that will be used to store persistent
  // files that should not be deleted, such as Opstats.
  virtual std::string GetBaseDir() = 0;

  // Returns the path of the directory that may be used to store
  // temporary/cached files. The federated compute runtime will use this
  // directory to cache data for re-use across multiple invocations, as well as
  // for creating temporary files that are deleted at the end of each
  // invocation. Implementers of this interface may also delete files in this
  // directory (for example, in low storage situations) without adverse effects
  // to the runtime. GetCacheDir() may return the same path as GetBaseDir().
  virtual std::string GetCacheDir() = 0;

  // TODO(team): factor out native implementations of this and delete.
  virtual absl::StatusOr<std::unique_ptr<ExampleIterator>>
  CreateExampleIterator(
      const google::internal::federated::plan::ExampleSelector&
          example_selector) = 0;

  virtual absl::StatusOr<std::unique_ptr<ExampleIterator>>
  CreateExampleIterator(
      const google::internal::federated::plan::ExampleSelector&
          example_selector,
      const SelectorContext& selector_context) {
    return CreateExampleIterator(example_selector);
  }

  // Creates an HttpClient. May return a nullptr if HTTP is not supported
  // (although support for HTTP will become mandatory in the future).
  virtual std::unique_ptr<fcp::client::http::HttpClient> CreateHttpClient() {
    return nullptr;
  }

  // Creates a verifier which can be used to verify a ConfidentialAggregations
  // service's attestation evidence. An AlwaysFailingAttestationVerifier will be
  // used by default (effectively disabling support for the
  // ConfidentialAggregations protocol). Must never return a nullptr.
  virtual std::unique_ptr<fcp::client::attestation::AttestationVerifier>
  CreateAttestationVerifier() {
    return std::make_unique<
        fcp::client::attestation::AlwaysFailingAttestationVerifier>();
  }

  // Checks whether the caller should abort computation. If less than
  // condition_polling_period time has elapsed since the last call this function
  // made to TrainingConditionsSatisfied, returns false.
  bool ShouldAbort(absl::Time current_time,
                   absl::Duration condition_polling_period);

  // This method is called when a task completes. It's called right after a
  // task's upload has finished, except if the task failed then it's called
  // right after the failure.  This method might be called multiple times if
  // more than one tasks were assigned to the device.
  virtual bool OnTaskCompleted(const TaskResultInfo& task_result_info) {
    return true;
  }

  virtual AccessPolicyEndorsementOptions GetAccessPolicyEndorsementOptions() {
    return AccessPolicyEndorsementOptions::default_instance();
  }

  virtual absl::StatusOr<std::string> GetAttestationMeasurement(
      std::string content_binding) {
    return "";
  }

 private:
  virtual bool TrainingConditionsSatisfied() = 0;

  absl::Time last_training_conditions_fetch_timestamp_ = absl::InfinitePast();
};

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_SIMPLE_TASK_ENVIRONMENT_H_
