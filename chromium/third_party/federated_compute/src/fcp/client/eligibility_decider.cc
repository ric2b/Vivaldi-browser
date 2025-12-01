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

#include "fcp/client/eligibility_decider.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "fcp/base/clock.h"
#include "fcp/base/digest.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/time_util.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/example_query_result.pb.h"
#include "fcp/client/flags.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/opstats/opstats_utils.h"
#include "fcp/client/phase_logger.h"
#include "fcp/client/selector_context.pb.h"
#include "fcp/client/simple_task_environment.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/protos/population_eligibility_spec.pb.h"
#include "re2/re2.h"
#include "tensorflow/core/example/example.pb.h"
#include "tensorflow/core/example/feature.pb.h"

namespace fcp::client {

using ::fcp::client::ExampleQueryResult;
using ::google::internal::federated::plan::DataAvailabilityPolicy;
using ::google::internal::federated::plan::EligibilityPolicyEvalSpec;
using ::google::internal::federated::plan::ExampleSelector;
using ::google::internal::federated::plan::MinimumSeparationPolicy;
using ::google::internal::federated::plan::PopulationEligibilitySpec;
using ::google::internal::federated::plan::SamplingWithoutReplacementPolicy;
using ::google::internal::federatedml::v2::TaskEligibilityInfo;
using ::google::internal::federatedml::v2::TaskWeight;

namespace {

const int32_t kDataAvailabilityImplementationVersionWithExampleQueryResult = 2;
const int32_t kSworImplementationVersion = 1;
const int32_t kTfCustomPolicyImplementationVersion = 1;
const int32_t kMinimumSeparationPolicyImplementationVersion = 1;
const int32_t kMinimumSeparationPolicyImplementationVersionWithPhaseStats = 3;

// URI handled by the eligibility decider for an example selector that will
// return a single example containing the name of the policy implementation to
// be evaluated and the task names that policy applies to.
constexpr char kNeetContextUri[] = "internal:/eligibility_context";
// The feature name key corresponding to the name of the policy impl to be
// evaluated in the example returned by the example iterator from the above URI.
constexpr char kPolicyName[] = "policy_name";
// The feature name key corresponding to the task names that the policy impl
// should be applied to.
constexpr char kTaskNames[] = "task_names";

bool ExecutionOutsideSworPeriod(
    absl::Time now, absl::Duration min_period,
    google::protobuf::Timestamp last_successful_contribution_time) {
  absl::Time last_successful_contribution_timestamp =
      fcp::TimeUtil::ConvertProtoToAbslTime(last_successful_contribution_time);
  absl::Duration period_between_last_successful_contribution =
      now - last_successful_contribution_timestamp;
  return min_period < period_between_last_successful_contribution;
}

// Computes sampling without replacement eligibility for the given set of tasks
// based on the execution history in opstats.
absl::flat_hash_set<std::string> ComputePerTaskSworEligibility(
    const SamplingWithoutReplacementPolicy& swor_policy,
    const absl::flat_hash_set<std::string>& task_names,
    const opstats::OpStatsSequence& opstats_sequence, Clock& clock) {
  // First, check that the period we're looking for does not exceed the opstats
  // db's trustworthiness timestamp. This timestamp tracks when the opstats db
  // was initialized, so if the swor period covers a time span beyond this
  // timestamp, it can not be guaranteed that the client did not successfully
  // participate within the period.
  absl::Duration min_period =
      fcp::TimeUtil::ConvertProtoToAbslDuration(swor_policy.min_period());
  absl::Time now = clock.Now();
  absl::Duration trustworthiness_period =
      now - fcp::TimeUtil::ConvertProtoToAbslTime(
                opstats_sequence.earliest_trustworthy_time());
  if (trustworthiness_period < min_period) {
    // No tasks eligible, return empty set
    return {};
  }

  // For group swor policies, since the period is consistent across all tasks,
  // and all tasks are in the same group, any previous successful contribution
  // matching the group_regex will opt out all tasks.
  if (!swor_policy.group_regex().empty()) {
    RE2 compiled_pattern(swor_policy.group_regex());
    if (!compiled_pattern.ok()) {
      // Bug in the group pattern, opt out all tasks. This should never happen
      // with the existing toolkit apis.
      return {};
    }
    std::optional<google::protobuf::Timestamp>
        last_successful_contribution_time =
            opstats::GetLastSuccessfulContributionTimeForPattern(
                opstats_sequence, compiled_pattern);

    // If there's no contribution by a task in the group, or if there are no
    // tasks in the group with an execution within the period, all tasks are
    // eligible.
    if (!last_successful_contribution_time.has_value() ||
        ExecutionOutsideSworPeriod(now, min_period,
                                   last_successful_contribution_time.value())) {
      return task_names;
    } else {
      return {};
    }
  }

  absl::flat_hash_set<std::string> eligibility_results;
  for (const std::string& task_name : task_names) {
    // TODO(team) Instead of using GetLastSuccessfulContributionTime
    // directly, consider passing in an opstats utils wrapper that can be
    // mocked.
    std::optional<google::protobuf::Timestamp>
        last_successful_contribution_time =
            opstats::GetLastSuccessfulContributionTime(opstats_sequence,
                                                       task_name);

    // If there was no last successful contribution time, we've never executed
    // this task.
    if (!last_successful_contribution_time.has_value()) {
      eligibility_results.insert(task_name);
      continue;
    }

    if (ExecutionOutsideSworPeriod(now, min_period,
                                   last_successful_contribution_time.value())) {
      // Most recent execution was outside the period, so we are eligible
      eligibility_results.insert(task_name);
      continue;
    }
  }

  return eligibility_results;
}

engine::ExampleIteratorFactory* FindExampleIteratorFactory(
    const ExampleSelector& selector,
    std::vector<engine::ExampleIteratorFactory*> example_iterator_factories) {
  for (engine::ExampleIteratorFactory* factory : example_iterator_factories) {
    // All ExampleIteratorFactories should be valid and never null.
    if (factory->CanHandle(selector)) {
      return factory;
    }
  }
  return nullptr;
}

// Returns true if the client has enough data to satisfy the policy, otherwise
// returns false. Returns an error status for exceptional cases.
absl::StatusOr<bool> ComputeDataAvailabilityEligibility(
    const DataAvailabilityPolicy& data_availability_policy,
    std::vector<engine::ExampleIteratorFactory*> example_iterator_factories,
    const Flags& flags) {
  const ExampleSelector& selector = data_availability_policy.selector();
  engine::ExampleIteratorFactory* iterator_factory =
      FindExampleIteratorFactory(selector, example_iterator_factories);

  if (iterator_factory == nullptr) {
    // The client does not host an iterator factory that can handle the given
    // selector.
    return absl::InvalidArgumentError(
        absl::StrCat("No iterator factory that can handle selector with uri ",
                     selector.collection_uri()));
  }
  SelectorContext selector_context = iterator_factory->GetSelectorContext();
  // We calculate the computation id here as a hash of the selection criteria.
  // Notice that it differs from the logic behind computing ids for regular
  // tasks, where id represents either the hash of all criteria combined (for
  // `ExampleQuerySpec` tasks), or the hash of the plan proto (for
  // `TensorFlowSpec` tasks). Each data availability policy is logically a
  // separate task thus having a separate identifier.
  selector_context.mutable_computation_properties()
      ->mutable_eligibility_eval()
      ->set_computation_id(
          ComputeSHA256(selector.criteria().SerializeAsString()));
  bool use_example_query_result_format =
      data_availability_policy.use_example_query_result_format();
  if (use_example_query_result_format) {
    selector_context.mutable_computation_properties()
        ->set_example_iterator_output_format(
            QueryTimeComputationProperties::EXAMPLE_QUERY_RESULT);
  }

  FCP_ASSIGN_OR_RETURN(
      std::unique_ptr<ExampleIterator> iterator,
      iterator_factory->CreateExampleIterator(selector, selector_context));
  int32_t min_example_count = data_availability_policy.min_example_count();
  if (use_example_query_result_format) {
    FCP_ASSIGN_OR_RETURN(std::string result, iterator->Next());
    ExampleQueryResult example_query_result;
    if (!example_query_result.ParseFromString(result)) {
      return absl::InvalidArgumentError("Failed to parse ExampleQueryResult");
    }
    return example_query_result.stats().output_rows_count() >=
           min_example_count;
  } else {
    int examples_returned = 0;
    while (examples_returned < min_example_count) {
      absl::StatusOr<std::string> example = iterator->Next();
      // Next() returns the example bytestring on success, and:
      //  - CANCELLED if the call got interrupted.
      //  - INVALID_ARGUMENT if some other error occurred, e.g. I/O.
      //  - OUT_OF_RANGE if the end of the iterator was reached.
      //
      // Only on success and OUT_OF_RANGE should we continue execution.
      if (example.ok()) {
        examples_returned++;
        continue;
      }
      if (absl::IsOutOfRange(example.status())) {
        // No more examples, and the client did not return a sufficient amount
        // to satisfy the policy.
        return false;
      }
      // By here, the example is not ok, and the status is unexpected.
      return example.status();
    }
  }

  // Enough examples returned!
  return true;
}

// An iterator that passes the name of the policy implementation to be executed
// and the task names it applies to to the TfCustomPolicy eligibility
// computation.
class EligibilityContextIterator : public fcp::client::ExampleIterator {
 public:
  explicit EligibilityContextIterator(std::string policy_name,
                                      std::vector<std::string> task_names) {
    tensorflow::Example example;
    auto* feature_map = example.mutable_features()->mutable_feature();
    (*feature_map)[kPolicyName] = CreateFeatureFromStrings({policy_name});
    (*feature_map)[kTaskNames] = CreateFeatureFromStrings(task_names);
    example_ = example.SerializeAsString();
  }

  absl::StatusOr<std::string> Next() override {
    if (example_returned_) {
      return absl::OutOfRangeError("Already returned neet context example");
    }
    example_returned_ = true;
    return example_;
  }

  void Close() override { example_returned_ = true; }

  tensorflow::Feature CreateFeatureFromStrings(
      std::vector<std::string> strings) {
    tensorflow::Feature feature;
    auto* bytes_list = feature.mutable_bytes_list();
    for (const std::string& str : strings) {
      bytes_list->add_value(str);
    }
    return feature;
  }

  bool example_returned_ = false;
  std::string example_;
};

std::unique_ptr<engine::ExampleIteratorFactory>
CreateEligibilityContextExampleIteratorFactory(
    std::string policy_name, std::vector<std::string> task_names) {
  return std::make_unique<engine::FunctionalExampleIteratorFactory>(
      /*can_handle_func=*/
      [](const google::internal::federated::plan::ExampleSelector& selector) {
        return selector.collection_uri() == kNeetContextUri;
      },
      /*create_iterator_func=*/
      [policy_name,
       task_names](const google::internal::federated::plan::ExampleSelector&
                       example_selector) {
        return std::make_unique<EligibilityContextIterator>(policy_name,
                                                            task_names);
      },
      /*should_collect_stats=*/false);
}

absl::StatusOr<absl::flat_hash_set<std::string>>
ComputeTfCustomPolicyEligibility(
    EligibilityPolicyEvalSpec policy_spec,
    const absl::flat_hash_set<std::string>& task_names,
    std::vector<engine::ExampleIteratorFactory*> example_iterator_factories,
    EetPlanRunner& eet_plan_runner) {
  std::vector<std::string> iterator_task_names(task_names.cbegin(),
                                               task_names.cend());
  std::unique_ptr<engine::ExampleIteratorFactory>
      neet_context_iterator_factory =
          CreateEligibilityContextExampleIteratorFactory(policy_spec.name(),
                                                         iterator_task_names);
  std::vector<engine::ExampleIteratorFactory*>
      overridden_example_iterator_factories = {
          neet_context_iterator_factory.get()};
  overridden_example_iterator_factories.insert(
      overridden_example_iterator_factories.end(),
      example_iterator_factories.begin(), example_iterator_factories.end());

  FCP_ASSIGN_OR_RETURN(
      TaskEligibilityInfo task_eligibility_info,
      eet_plan_runner.RunPlan(overridden_example_iterator_factories));
  absl::flat_hash_set<std::string> eligible_task_names = {};
  for (const TaskWeight& task_weight : task_eligibility_info.task_weights()) {
    if (task_weight.weight() > 0) {
      eligible_task_names.insert(task_weight.task_name());
    }
  }
  return eligible_task_names;
}

int32_t GetMinSepImplementationVersion(const Flags* flags) {
  if (flags->log_min_sep_index_to_phase_stats()) {
    return kMinimumSeparationPolicyImplementationVersionWithPhaseStats;
  }
  return kMinimumSeparationPolicyImplementationVersion;
}

// Computes minimum separation eligibility for the given set of tasks based on
// the execution history in opstats.
absl::flat_hash_set<std::string> ComputeMinimumSeparationPolicyEligibility(
    const MinimumSeparationPolicy& min_sep_policy,
    const absl::flat_hash_set<std::string>& task_names,
    const opstats::OpStatsSequence& opstats_sequence, Clock& clock,
    const Flags& flags, LogManager& log_manager) {
  if (flags.check_trustworthiness_for_min_sep_policy()) {
    // First, check that the policy's min trustworthiness period does not exceed
    // the opstats db's trustworthiness timestamp if it is set. This timestamp
    // tracks when the opstats db was initialized, so if the required min
    // trustworthiness period covers a time span beyond this timestamp, it can
    // not be guaranteed that the client did not successfully participate within
    // the period.
    absl::Duration min_period = fcp::TimeUtil::ConvertProtoToAbslDuration(
        min_sep_policy.min_trustworthiness_period());
    if (min_period != absl::ZeroDuration()) {
      absl::Time now = clock.Now();
      absl::Duration trustworthiness_period =
          now - fcp::TimeUtil::ConvertProtoToAbslTime(
                    opstats_sequence.earliest_trustworthy_time());
      if (trustworthiness_period < min_period) {
        // No tasks eligible, return empty set
        return {};
      }
    }
  }

  absl::flat_hash_set<std::string> eligibility_results;
  for (const std::string& task_name : task_names) {
    std::optional<int64_t> last_successful_contribution_index =
        opstats::GetLastSuccessfulContributionMinSepPolicyIndex(
            opstats_sequence, task_name);

    // If there was no last successful contribution index, we've never executed
    // this task with this policy.
    if (!last_successful_contribution_index.has_value()) {
      eligibility_results.insert(task_name);
      continue;
    }

    // Note that the index separation is defined to be the number of indexes
    // (exclusive) between two consecutive contributions.
    int64_t index_separation = min_sep_policy.current_index() -
                               last_successful_contribution_index.value() - 1;
    if (index_separation >= min_sep_policy.minimum_separation()) {
      eligibility_results.insert(task_name);
      log_manager.LogToLongHistogram(
          HistogramCounters::TRAINING_FL_ROUND_SEPARATION, index_separation);
    }
  }

  return eligibility_results;
}

}  // namespace

absl::StatusOr<TaskEligibilityInfo> ComputeEligibility(
    const PopulationEligibilitySpec& population_eligibility_spec,
    LogManager& log_manager, PhaseLogger& phase_logger,
    const opstats::OpStatsSequence& opstats_sequence, Clock& clock,
    std::vector<engine::ExampleIteratorFactory*> example_iterator_factories,
    EetPlanRunner& eet_plan_runner, const Flags* flags) {
  // Initialize the TaskEligibilityInfo to return. If eligibility cannot be
  // decided, i.e. due to insufficient implementations, we'll return this
  // unfilled.
  TaskEligibilityInfo eligibility_result;

  // Initialize map of policy name -> task names that use that policy, and check
  // that the implementation versions for each policy are supported by the
  // client.
  // This map must only contain names of policies that are implemented.
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>
      policy_name_to_task_names;

  // Track task names that use unimplemented policies. After we build the policy
  // name -> task name map, we'll prune any task names that use unimplemented
  // policies. We can't do this until after we've built the whole map.
  absl::flat_hash_set<std::string> task_names_using_unimplemented_policies;

  for (int policy_index = 0;
       policy_index < population_eligibility_spec.eligibility_policies_size();
       policy_index++) {
    bool policy_implemented = true;
    const EligibilityPolicyEvalSpec& policy_spec =
        population_eligibility_spec.eligibility_policies(policy_index);

    switch (policy_spec.policy_type_case()) {
      case EligibilityPolicyEvalSpec::PolicyTypeCase::kDataAvailabilityPolicy:
        if (kDataAvailabilityImplementationVersionWithExampleQueryResult <
            policy_spec.min_version()) {
          policy_implemented = false;
        }
        break;
      case EligibilityPolicyEvalSpec::PolicyTypeCase::kSworPolicy:
        if (kSworImplementationVersion < policy_spec.min_version()) {
          policy_implemented = false;
        }
        break;
      case EligibilityPolicyEvalSpec::PolicyTypeCase::kTfCustomPolicy:
        if (kTfCustomPolicyImplementationVersion < policy_spec.min_version()) {
            policy_implemented = false;
        }
        break;
      case EligibilityPolicyEvalSpec::PolicyTypeCase::kMinSepPolicy:
        if (GetMinSepImplementationVersion(flags) < policy_spec.min_version()) {
          policy_implemented = false;
        }
        break;
      default:
        // Unknown or unset policy type! This can happen if a new policy type
        // has been added on the server, but is not yet implemented in the
        // client.
          policy_implemented = false;
    }

    absl::flat_hash_set<std::string> task_names;
    for (const auto& task_info : population_eligibility_spec.task_info()) {
      for (int i : task_info.eligibility_policy_indices()) {
        if (policy_index == i) {
          if (!policy_implemented) {
            task_names_using_unimplemented_policies.insert(
                task_info.task_name());
          } else {
            task_names.insert(task_info.task_name());
          }
        }
      }
    }
    if (!policy_implemented) {
      continue;
    }
    policy_name_to_task_names[policy_spec.name()] = task_names;
  }

  // Initialize the list of task names to compute eligibility for. Task names
  // will be removed from this list as policy computations mark them as
  // ineligible. For each policy computation, we only evaluate the intersection
  // of the tasks that use that policy and this list. This avoids computing
  // eligibility for tasks which have already been marked ineligible from
  // previous computations.
  absl::flat_hash_set<std::string> eligible_tasks;
  for (const auto& task_info : population_eligibility_spec.task_info()) {
    eligible_tasks.insert(task_info.task_name());
  }

  // If we have any task names that use unimplemented policies, remove them from
  // eligible_tasks.
  if (!task_names_using_unimplemented_policies.empty()) {
    absl::erase_if(eligible_tasks, [&task_names_using_unimplemented_policies](
                                       const std::string& task_name) {
      return task_names_using_unimplemented_policies.contains(task_name);
    });
  }

  // For each policy:
  // 1. If eligible tasks is now empty due to the previous iteration, quit
  // early.
  // 2. Get task names from policy_name_to_task_names.
  // 3. Intersect with eligible_tasks to get the set of tasks names that we
  // should use to compute eligibility for this policy.
  // 4. Call compute x policy, get back a set of task names. This set will only
  // contain tasks that are still eligible according to the policy computation
  // we just did.
  // 5. Remove all tasks from eligible_tasks that are not eligible according to
  // the policy computation.

  for (const EligibilityPolicyEvalSpec& policy_spec :
       population_eligibility_spec.eligibility_policies()) {
    // Check if there are no more eligible tasks due to previous policy
    // computations. If so, there's no point in continuing to evaluate policies.
    if (eligible_tasks.empty()) {
      break;
    }

    // policy_name_to_task_names contains our map of *implemented* policies to
    // the tasks that use them. First check if it's in the map. If not, there's
    // no point in evaulating this policy.
    if (!policy_name_to_task_names.contains(policy_spec.name())) {
      continue;
    }

    // The task names this policy applies to.
    absl::flat_hash_set<std::string> policy_task_names =
        policy_name_to_task_names.at(policy_spec.name());
    // Remove those tasks that already aren't eligible anymore.
    absl::erase_if(policy_task_names,
                   [&eligible_tasks](const std::string& policy_task_name) {
                     return !eligible_tasks.contains(policy_task_name);
                   });

    // If policy_task_names is now empty, there's no need to compute eligibility
    // for this policy.
    if (policy_task_names.empty()) {
      continue;
    }

    absl::flat_hash_set<std::string> eligible_policy_task_names;
    switch (policy_spec.policy_type_case()) {
      case EligibilityPolicyEvalSpec::PolicyTypeCase::kSworPolicy:
        eligible_policy_task_names = ComputePerTaskSworEligibility(
            policy_spec.swor_policy(), policy_task_names, opstats_sequence,
            clock);
        break;
      case EligibilityPolicyEvalSpec::PolicyTypeCase::kDataAvailabilityPolicy:
        // Need to wrap initialization of local variables in a case statement.
        {
          absl::StatusOr<bool> data_is_available =
              ComputeDataAvailabilityEligibility(
                  policy_spec.data_availability_policy(),
                  example_iterator_factories, *flags);
          if (data_is_available.ok()) {
            if (*data_is_available) {
              // Data is available for all tasks that use this policy.
              eligible_policy_task_names = policy_task_names;
            }
            // No tasks are eligible, so leave eligible_policy_task_names empty.
          } else {
            phase_logger.LogEligibilityEvalComputationErrorNonfatal(
                data_is_available.status());
            // No tasks are eligible, so leave eligible_policy_task_names
            // empty. Below, we'll remove them from the overall eligible
            // tasks.
          }
        }
        break;
      case EligibilityPolicyEvalSpec::PolicyTypeCase::kTfCustomPolicy: {
        absl::StatusOr<absl::flat_hash_set<std::string>> tf_policy_task_names =
            ComputeTfCustomPolicyEligibility(policy_spec, policy_task_names,
                                             example_iterator_factories,
                                             eet_plan_runner);
        if (tf_policy_task_names.ok()) {
          eligible_policy_task_names = *tf_policy_task_names;
        } else {
          phase_logger.LogEligibilityEvalComputationErrorNonfatal(
              tf_policy_task_names.status());
          // No tasks are eligible, so leave eligible_policy_task_names empty.
          // Below, we'll remove them from the overall eligible tasks.
        }
      } break;
      case EligibilityPolicyEvalSpec::PolicyTypeCase::kMinSepPolicy: {
        eligible_policy_task_names = ComputeMinimumSeparationPolicyEligibility(
            policy_spec.min_sep_policy(), policy_task_names, opstats_sequence,
            clock, *flags, log_manager);
      } break;
      default:
        // Should never happen, because we pre-filtered above based on
        // unimplemented policy.
        return absl::InternalError(
            absl::StrCat("Unexpected policy kind during eval: ",
                         policy_spec.policy_type_case()));
    }

    // Merge computed eligible tasks with overall eligible tasks. If a task name
    // is in policy_task_names but not in eligible_policy_task_names,
    // the runtime is not eligible for that task.
    for (const std::string& task_name : policy_task_names) {
      if (!eligible_policy_task_names.contains(task_name)) {
        eligible_tasks.erase(task_name);
      }
    }
  }

  // Now, eligible_tasks only contains task names that are eligible for all of
  // their applied policies.
  eligibility_result.set_version(1);
  for (const auto& task_info : population_eligibility_spec.task_info()) {
    TaskWeight* task_weight = eligibility_result.mutable_task_weights()->Add();
    task_weight->set_task_name(task_info.task_name());
    task_weight->set_weight(
        eligible_tasks.contains(task_info.task_name()) ? 1.0f : 0);
  }
  return eligibility_result;
}

}  // namespace fcp::client
