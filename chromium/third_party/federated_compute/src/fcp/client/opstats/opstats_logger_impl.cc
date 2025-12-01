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
#include "fcp/client/opstats/opstats_logger_impl.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/util/time_util.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/time_util.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/flags.h"
#include "fcp/client/histogram_counters.pb.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/opstats/opstats_db.h"
#include "fcp/client/opstats/opstats_logger.h"
#include "fcp/client/opstats/pds_backed_opstats_db.h"
#include "fcp/client/stats.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/protos/opstats.pb.h"

namespace fcp {
namespace client {
namespace opstats {

using ::google::internal::federatedml::v2::RetryWindow;

OpStatsLoggerImpl::OpStatsLoggerImpl(std::unique_ptr<OpStatsDb> db,
                                     LogManager* log_manager,
                                     const Flags* flags,
                                     const std::string& session_name,
                                     const std::string& population_name)
    : db_(std::move(db)),
      log_manager_(log_manager),
      log_min_sep_index_to_phase_stats_(
          flags->log_min_sep_index_to_phase_stats()) {
  log_manager_->LogDiag(DebugDiagCode::TRAINING_OPSTATS_ENABLED);
  log_manager_->LogDiag(ProdDiagCode::OPSTATS_DB_COMMIT_EXPECTED);

  // Setup the OperationalStats message for the new run.
  stats_.set_session_name(session_name);
  stats_.set_population_name(population_name);
}

OpStatsLoggerImpl::~OpStatsLoggerImpl() {
  // We're in the dtor, we don't care about what CommitToStorage returns.
  auto status = CommitToStorage();
}

bool OpStatsLoggerImpl::IsInitializationEvent(
    OperationalStats::Event::EventKind kind) {
  return kind == OperationalStats::Event::EVENT_KIND_TRAIN_NOT_STARTED ||
         kind ==
             OperationalStats::Event::EVENT_KIND_INITIALIZATION_ERROR_FATAL ||
         kind ==
             OperationalStats::Event::EVENT_KIND_INITIALIZATION_ERROR_NONFATAL;
}

void OpStatsLoggerImpl::AddEventAndSetTaskName(
    const std::string& task_name, OperationalStats::Event::EventKind event) {
  absl::MutexLock lock(&mutex_);
  FCP_CHECK(current_phase_stats_.phase() !=
            OperationalStats::PhaseStats::UNSPECIFIED)
      << "AddEventAndSetTaskName called before StartLoggingForPhase";

  AddNewEventToCurrentPhaseStats(event);
  current_phase_stats_.set_task_name(task_name);
}

void OpStatsLoggerImpl::AddEvent(OperationalStats::Event::EventKind event) {
  absl::MutexLock lock(&mutex_);
  if (current_phase_stats_.phase() !=
      OperationalStats::PhaseStats::UNSPECIFIED) {
    AddNewEventToCurrentPhaseStats(event);
  } else {
    FCP_CHECK(IsInitializationEvent(event))
        << "AddEvent called before StartLoggingForPhase";
    // Initialization events don't belong to any phase, we'll log it at the
    // top level of an OperationalStats message.
    AddNewEventToStats(event);
  }
}

void OpStatsLoggerImpl::AddEventWithErrorMessage(
    OperationalStats::Event::EventKind event,
    const std::string& error_message) {
  absl::MutexLock lock(&mutex_);
  if (current_phase_stats_.phase() !=
      OperationalStats::PhaseStats::UNSPECIFIED) {
    AddNewEventToCurrentPhaseStats(event);
    // Don't replace an existing error message.
    if (current_phase_stats_.error_message().empty()) {
      current_phase_stats_.set_error_message(error_message);
    }
  } else {
    FCP_CHECK(IsInitializationEvent(event))
        << "AddEventWithErrorMessage called before "
        << "StartLoggingForPhase";
    AddNewEventToStats(event);
    // Don't replace an existing error message.
    if (stats_.error_message().empty()) {
      stats_.set_error_message(error_message);
    }
  }
}

void OpStatsLoggerImpl::SetMinSepPolicyIndex(int64_t current_index) {
  absl::MutexLock lock(&mutex_);
  if (log_min_sep_index_to_phase_stats_) {
    FCP_CHECK(current_phase_stats_.phase() !=
              OperationalStats::PhaseStats::UNSPECIFIED)
        << "SetMinSepPolicyIndex called before StartLoggingForPhase";
    current_phase_stats_.set_min_sep_policy_index(current_index);
  } else {
    stats_.set_min_sep_policy_index(current_index);
  }
}

void OpStatsLoggerImpl::RecordCollectionFirstAccessTime(
    absl::string_view collection_uri, absl::Time first_access_time) {
  absl::MutexLock lock(&mutex_);
  if (!collection_first_access_time_map_.contains(collection_uri)) {
    collection_first_access_time_map_[collection_uri] = first_access_time;
  }
}

void OpStatsLoggerImpl::UpdateDatasetStats(
    const std::string& collection_uri, int additional_example_count,
    int64_t additional_example_size_bytes) {
  absl::MutexLock lock(&mutex_);
  OperationalStats::DatasetStats* dataset_stats;
  FCP_CHECK(current_phase_stats_.phase() !=
            OperationalStats::PhaseStats::UNSPECIFIED)
      << "UpdateDatasetStats called before StartLoggingForPhase";
  dataset_stats =
      &(*current_phase_stats_.mutable_dataset_stats())[collection_uri];

  dataset_stats->set_num_examples_read(dataset_stats->num_examples_read() +
                                       additional_example_count);
  dataset_stats->set_num_bytes_read(dataset_stats->num_bytes_read() +
                                    additional_example_size_bytes);

  if (collection_first_access_time_map_.contains(collection_uri)) {
    *dataset_stats->mutable_first_access_timestamp() =
        TimeUtil::ConvertAbslToProtoTimestamp(
            collection_first_access_time_map_[collection_uri]);
  }
}

void OpStatsLoggerImpl::SetNetworkStats(const NetworkStats& network_stats) {
  absl::MutexLock lock(&mutex_);
  if (current_phase_stats_.phase() !=
      OperationalStats::PhaseStats::UNSPECIFIED) {
    // The input network stats is always the accumulated stats, but we want the
    // per-phase network stats in PhaseStats.  Therefore, we calculates the
    // difference between the new network stats with the accumulated network
    // stats, and add the difference to the network stats for the current phase.
    NetworkStats incremental_network_stats =
        network_stats - accumulated_network_stats_;
    current_phase_stats_.set_bytes_downloaded(
        current_phase_stats_.bytes_downloaded() +
        incremental_network_stats.bytes_downloaded);
    current_phase_stats_.set_bytes_uploaded(
        current_phase_stats_.bytes_uploaded() +
        incremental_network_stats.bytes_uploaded);
    *current_phase_stats_.mutable_network_duration() =
        current_phase_stats_.network_duration() +
        TimeUtil::ConvertAbslToProtoDuration(
            incremental_network_stats.network_duration);
    accumulated_network_stats_ = network_stats;
  } else {
    stats_.set_chunking_layer_bytes_downloaded(network_stats.bytes_downloaded);
    stats_.set_chunking_layer_bytes_uploaded(network_stats.bytes_uploaded);
    *stats_.mutable_network_duration() =
        TimeUtil::ConvertAbslToProtoDuration(network_stats.network_duration);
  }
}

// NOTE: unlike other methods in this file where we force the caller to call
// StartLoggingForPhase before calling the method, this method can be called any
// time.
void OpStatsLoggerImpl::SetRetryWindow(RetryWindow retry_window) {
  absl::MutexLock lock(&mutex_);
  retry_window.clear_retry_token();
  *stats_.mutable_retry_window() = std::move(retry_window);
}

// This method is only used when not logging PhaseStats.
void OpStatsLoggerImpl::AddNewEventToStats(
    OperationalStats::Event::EventKind kind) {
  auto new_event = stats_.add_events();
  new_event->set_event_type(kind);
  *new_event->mutable_timestamp() = google::protobuf::util::TimeUtil::GetCurrentTime();
}

void OpStatsLoggerImpl::AddNewEventToCurrentPhaseStats(
    OperationalStats::Event::EventKind kind) {
  auto new_event = current_phase_stats_.add_events();
  new_event->set_event_type(kind);
  *new_event->mutable_timestamp() = google::protobuf::util::TimeUtil::GetCurrentTime();
}

// NOTE: unlike many other methods in this file where we force the caller to
// call StartLoggingForPhase before calling the method, this method can be
// called any time.
absl::Status OpStatsLoggerImpl::CommitToStorage() {
  absl::MutexLock lock(&mutex_);
  log_manager_->LogDiag(ProdDiagCode::OPSTATS_DB_COMMIT_ATTEMPTED);
  const absl::Time before_commit_time = absl::Now();
  OperationalStats copy = stats_;
  if (current_phase_stats_.phase() !=
      OperationalStats::PhaseStats::UNSPECIFIED) {
    *copy.add_phase_stats() = current_phase_stats_;
  }
  auto status = already_committed_
                    ? db_->Transform([&copy](OpStatsSequence& data) {
                        // Check if opstats on disk somehow got cleared between
                        // the first commit and now, and handle appropriately.
                        // This can happen e.g. if the ttl for the opstats db
                        // is incorrectly configured to have a very low ttl,
                        // causing the entire history to be lost as part of the
                        // update.
                        if (data.opstats_size() == 0) {
                          *data.add_opstats() = copy;
                        } else {
                          *data.mutable_opstats(data.opstats_size() - 1) = copy;
                        }
                      })
                    : db_->Transform([&copy](OpStatsSequence& data) {
                        *data.add_opstats() = copy;
                      });
  const absl::Time after_commit_time = absl::Now();
  log_manager_->LogToLongHistogram(
      HistogramCounters::TRAINING_OPSTATS_COMMIT_LATENCY,
      absl::ToInt64Milliseconds(after_commit_time - before_commit_time));
  already_committed_ = true;
  return status;
}

std::string OpStatsLoggerImpl::GetCurrentTaskName() {
  absl::MutexLock lock(&mutex_);
  if (!current_phase_stats_.task_name().empty()) {
    return current_phase_stats_.task_name();
  }
  for (auto it = stats_.phase_stats().rbegin();
       it != stats_.phase_stats().rend(); it++) {
    if (!it->task_name().empty()) {
      return it->task_name();
    }
  }
  // If we reach this line, it means we don't know the task name yet for the
  // current run. Technically, it won't happen because this method is only
  // called by the opstats example store and at that point we already know
  // the task name.  Unfortunately, we can't enforce this at API level.
  return "";
}

void OpStatsLoggerImpl::StartLoggingForPhase(
    OperationalStats::PhaseStats::Phase phase) {
  absl::MutexLock lock(&mutex_);

  if (current_phase_stats_.phase() !=
      OperationalStats::PhaseStats::UNSPECIFIED) {
    // The user didn't stop the logging for the previous Phase. We'll add the
    // cached PhaseStats to the stats first.
    *stats_.add_phase_stats() = current_phase_stats_;
    current_phase_stats_ = OperationalStats::PhaseStats();
  }
  current_phase_stats_.set_phase(phase);
}

void OpStatsLoggerImpl::StopLoggingForTheCurrentPhase() {
  absl::MutexLock lock(&mutex_);

  // Only add the current PhaseStats when it's not empty.
  FCP_CHECK(current_phase_stats_.phase() !=
            OperationalStats::PhaseStats::UNSPECIFIED)
      << "StopLoggingForTheCurrentPhase called before StartLoggingForPhase";
  *stats_.add_phase_stats() = current_phase_stats_;
  current_phase_stats_ = OperationalStats::PhaseStats();
}

std::unique_ptr<OpStatsLogger> CreateOpStatsLogger(
    const std::string& base_dir, const Flags* flags, LogManager* log_manager,
    const std::string& session_name, const std::string& population_name) {
  auto db_or = PdsBackedOpStatsDb::Create(
      base_dir, flags->opstats_ttl_days() * absl::Hours(24), *log_manager,
      flags->opstats_db_size_limit_bytes());
  if (db_or.ok()) {
    return std::make_unique<OpStatsLoggerImpl>(std::move(db_or).value(),
                                               log_manager, flags, session_name,
                                               population_name);
  } else {
    return std::make_unique<OpStatsLogger>(
        /*init_status=*/db_or.status());
  }
}

}  // namespace opstats
}  // namespace client
}  // namespace fcp
