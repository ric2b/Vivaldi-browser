// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/component.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/update_client/action_runner.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/features.h"
#include "components/update_client/network.h"
#include "components/update_client/patcher.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_serializer.h"
#include "components/update_client/puffin_patcher.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/unpacker.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_client_metrics.h"
#include "components/update_client/update_engine.h"
#include "components/update_client/utils.h"

// The state machine representing how a CRX component changes during an update.
//
//     +------------------------- kNew
//     |                            |
//     |                            V
//     |                        kChecking
//     |                            |
//     V                error       V     no           no
//  kUpdateError <------------- [update?] -> [action?] -> kUpToDate
//     ^                            |           |            ^
//     |                        yes |           | yes        |
//     |     update disabled        V           |            |
//     +-<--------------------- kCanUpdate      +--------> kRun
//     |                            |
//     |                            V           yes
//     |                    [download cached?] --------------+
//     |                               |                     |
//     |                            no |                     |
//     |                no             |                     |
//     |               +-<- [differential update?]           |
//     |               |               |                     |
//     |               |           yes |                     |
//     |               |               |                     |
//     |    error, no  |               |                     |
//     +-<----------[disk space available?]                  |
//     |               |               |                     |
//     |           yes |           yes |                     |
//     |               |               |                     |
//     |               |               |                     |
//     |               | error         V                     |
//     |               +-<----- kDownloadingDiff             |
//     |               |               |                     |
//     |               |               |                     |
//     |               | error         V                     |
//     |               +-<----- kUpdatingDiff                |
//     |               |               |                     |
//     |    error      V               |                     |
//     +-<-------- kDownloading        |                     |
//     |               |               |                     |
//     |               |               |                     |
//     |    error      V               V      no             |
//     +-<-------- kUpdating -----> [action?] -> kUpdated    |
//                     ^               |            ^        |
//                     |               | yes        |        |
//                     |               |            |        |
//                     |               +--------> kRun       |
//                     |                                     |
//                     +-------------------------------------+

// The state machine for a check for update only.
//
//                                kNew
//                                  |
//                                  V
//                             kChecking
//                                  |
//                         yes      V     no
//                         +----[update?] ------> kUpToDate
//                         |
//             yes         v           no
//          +---<-- update disabled? -->---+
//          |                              |
//     kUpdateError                    kCanUpdate

namespace update_client {
namespace {

using InstallOnBlockingTaskRunnerCompleteCallback = base::OnceCallback<void(
    ErrorCategory error_category,
    int error_code,
    int extra_code1,
    std::optional<CrxInstaller::Result> installer_result)>;

void InstallComplete(scoped_refptr<base::SequencedTaskRunner> main_task_runner,
                     InstallOnBlockingTaskRunnerCompleteCallback callback,
                     const base::FilePath& unpack_path,
                     const CrxInstaller::Result& installer_result) {
  base::ThreadPool::PostTask(
      FROM_HERE, kTaskTraits,
      base::BindOnce(
          [](scoped_refptr<base::SequencedTaskRunner> main_task_runner,
             InstallOnBlockingTaskRunnerCompleteCallback callback,
             const base::FilePath& unpack_path,
             const CrxInstaller::Result& installer_result) {
            base::DeletePathRecursively(unpack_path);
            const ErrorCategory error_category = installer_result.error
                                                     ? ErrorCategory::kInstaller
                                                     : ErrorCategory::kNone;
            main_task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), error_category,
                               static_cast<int>(installer_result.error),
                               installer_result.extended_error,
                               installer_result));
          },
          main_task_runner, std::move(callback), unpack_path,
          installer_result));
}

void InstallOnBlockingTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    const base::FilePath& unpack_path,
    const std::string& public_key,
    const std::string& fingerprint,
    std::unique_ptr<CrxInstaller::InstallParams> install_params,
    scoped_refptr<CrxInstaller> installer,
    CrxInstaller::ProgressCallback progress_callback,
    InstallOnBlockingTaskRunnerCompleteCallback callback) {
  VLOG_IF(1, !base::DirectoryExists(unpack_path))
      << unpack_path << " does not exist";

  base::ScopedTempDir unpack_path_owner;
  std::ignore = unpack_path_owner.Set(unpack_path);

  if (!base::WriteFile(
          unpack_path.Append(FILE_PATH_LITERAL("manifest.fingerprint")),
          fingerprint)) {
    const CrxInstaller::Result installer_result(
        InstallError::FINGERPRINT_WRITE_FAILED,
        logging::GetLastSystemErrorCode());
    main_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), ErrorCategory::kInstall,
                       static_cast<int>(installer_result.error),
                       installer_result.extended_error, std::nullopt));
    return;
  }

  // Ensures that progress callback is not posted after the completion
  // callback. There is a current design limitation in the update client where a
  // poorly implemented installer could post progress after the completion, and
  // thus, break some update client invariants.
  // Both callbacks maintain a reference to an instance of this class.
  // The state of the boolean atomic member is tested on the main sequence, and
  // the progress callback is posted to the sequence only if the completion
  // callback has not occurred yet.
  class CallbackChecker : public base::RefCountedThreadSafe<CallbackChecker> {
   public:
    bool is_safe() const { return is_safe_; }
    void set_unsafe() { is_safe_ = false; }

   private:
    friend class base::RefCountedThreadSafe<CallbackChecker>;
    ~CallbackChecker() = default;
    std::atomic<bool> is_safe_ = {true};
  };

  // Adapts the progress and completion callbacks such that the callback checker
  // is marked as unsafe before invoking the completion callback. On the
  // progress side, it allows reposting of the progress callback only when the
  // checker is in a safe state, as seen from the main sequence.
  auto callback_checker = base::MakeRefCounted<CallbackChecker>();
  installer->Install(
      unpack_path, public_key, std::move(install_params),
      base::BindRepeating(
          [](scoped_refptr<base::SequencedTaskRunner> main_task_runner,
             scoped_refptr<CallbackChecker> callback_checker,
             CrxInstaller::ProgressCallback progress_callback, int progress) {
            main_task_runner->PostTask(
                FROM_HERE,
                base::BindRepeating(
                    [](scoped_refptr<CallbackChecker> callback_checker,
                       CrxInstaller::ProgressCallback progress_callback,
                       int progress) {
                      if (callback_checker->is_safe()) {
                        progress_callback.Run(progress);
                      } else {
                        DVLOG(2) << "Progress callback was skipped.";
                      }
                    },
                    callback_checker, progress_callback, progress));
          },
          main_task_runner, callback_checker, progress_callback),
      base::BindOnce(
          [](scoped_refptr<CallbackChecker> callback_checker,
             CrxInstaller::Callback callback,
             const CrxInstaller::Result& installer_result) {
            callback_checker->set_unsafe();
            std::move(callback).Run(installer_result);
          },
          callback_checker,
          base::BindOnce(&InstallComplete, main_task_runner,
                         std::move(callback), unpack_path_owner.Take())));
}

void CrxCachePutCompleteOnCrxCacheBlockingTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    const base::FilePath& crx_path,
    const base::FilePath& unpack_path,
    const std::string& public_key,
    const std::string& fingerprint,
    std::unique_ptr<CrxInstaller::InstallParams> install_params,
    scoped_refptr<CrxInstaller> installer,
    CrxInstaller::ProgressCallback progress_callback,
    InstallOnBlockingTaskRunnerCompleteCallback callback,
    const CrxCache::Result& result) {
  if (result.error != UnpackerError::kNone) {
    update_client::DeleteFileAndEmptyParentDirectory(crx_path);
    DVLOG(2) << "crx_cache->Put failed: " << static_cast<int>(result.error);
  } else {
    update_client::DeleteEmptyDirectory(crx_path.DirName());
  }
  base::ThreadPool::PostTask(
      FROM_HERE, kTaskTraits,
      base::BindOnce(&InstallOnBlockingTaskRunner, main_task_runner,
                     unpack_path, public_key, fingerprint,
                     std::move(install_params), installer, progress_callback,
                     std::move(callback)));
}

void PuffinUnpackCompleteOnBlockingTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    const base::FilePath& crx_path,
    const std::string& id,
    const std::string& fingerprint,
    std::unique_ptr<CrxInstaller::InstallParams> install_params,
    scoped_refptr<CrxInstaller> installer,
    std::optional<scoped_refptr<update_client::CrxCache>> optional_crx_cache,
    CrxInstaller::ProgressCallback progress_callback,
    InstallOnBlockingTaskRunnerCompleteCallback callback,
    const Unpacker::Result& result) {
  if (result.error != UnpackerError::kNone) {
    update_client::DeleteFileAndEmptyParentDirectory(crx_path);
    DVLOG(2) << "Unpack failed: " << static_cast<int>(result.error);
    main_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), ErrorCategory::kUnpack,
                                  static_cast<int>(result.error),
                                  result.extended_error, std::nullopt));
  } else if (!optional_crx_cache.has_value()) {
    // If we were unable to create the crx_cache, skip the CrxCache::Put call.
    // Since we don't need the cache to perform full updates, ignore the error
    DVLOG(2) << "No crx cache provided, proceeding without crx retention.";
    update_client::DeleteFileAndEmptyParentDirectory(crx_path);
    base::ThreadPool::PostTask(
        FROM_HERE, kTaskTraits,
        base::BindOnce(&InstallOnBlockingTaskRunner, main_task_runner,
                       result.unpack_path, result.public_key, fingerprint,
                       std::move(install_params), installer, progress_callback,
                       std::move(callback)));
  } else {
    main_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CrxCache::Put, optional_crx_cache.value(), crx_path, id,
            fingerprint,
            base::BindPostTask(
                base::ThreadPool::CreateSequencedTaskRunner(kTaskTraits),
                base::BindOnce(&CrxCachePutCompleteOnCrxCacheBlockingTaskRunner,
                               main_task_runner, crx_path, result.unpack_path,
                               result.public_key, fingerprint,
                               std::move(install_params), installer,
                               progress_callback, std::move(callback)))));
  }
}

void StartPuffinInstallOnBlockingTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    const std::vector<uint8_t>& pk_hash,
    const base::FilePath& crx_path,
    const std::string& id,
    const std::string& fingerprint,
    std::unique_ptr<CrxInstaller::InstallParams> install_params,
    scoped_refptr<CrxInstaller> installer,
    std::unique_ptr<Unzipper> unzipper_,
    std::optional<scoped_refptr<update_client::CrxCache>> optional_crx_cache,
    crx_file::VerifierFormat crx_format,
    CrxInstaller::ProgressCallback progress_callback,
    InstallOnBlockingTaskRunnerCompleteCallback callback) {
  Unpacker::Unpack(
      pk_hash, crx_path, std::move(unzipper_), crx_format,
      base::BindOnce(&PuffinUnpackCompleteOnBlockingTaskRunner,
                     main_task_runner, crx_path, id, fingerprint,
                     std::move(install_params), installer, optional_crx_cache,
                     progress_callback, std::move(callback)));
}

void OnPuffPatchCompleteOnBlockingTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    const std::vector<uint8_t>& pk_hash,
    const base::FilePath& patch_path,
    const base::FilePath& dest_crx_path,
    const std::string& id,
    const std::string& fingerprint,
    std::unique_ptr<CrxInstaller::InstallParams> install_params,
    scoped_refptr<CrxInstaller> installer,
    std::unique_ptr<Unzipper> unzipper_,
    scoped_refptr<update_client::CrxCache> crx_cache,
    crx_file::VerifierFormat crx_format,
    CrxInstaller::ProgressCallback progress_callback,
    InstallOnBlockingTaskRunnerCompleteCallback callback,
    UnpackerError error,
    int extra_code) {
  update_client::DeleteFileAndEmptyParentDirectory(patch_path);
  if (error != UnpackerError::kNone) {
    update_client::DeleteFileAndEmptyParentDirectory(dest_crx_path);
    main_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), ErrorCategory::kUnpack,
                       static_cast<int>(error), extra_code, std::nullopt));
    DVLOG(2) << "PuffPatch failed: " << static_cast<int>(error);
    return;
  }

  base::ThreadPool::CreateSequencedTaskRunner(kTaskTraits)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              &update_client::StartPuffinInstallOnBlockingTaskRunner,
              main_task_runner, pk_hash, dest_crx_path, id, fingerprint,
              std::move(install_params), installer, std::move(unzipper_),
              std::optional<scoped_refptr<update_client::CrxCache>>(crx_cache),
              crx_format, progress_callback, std::move(callback)));
}

void StartPuffPatchOnBlockingTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    const std::vector<uint8_t>& pk_hash,
    const base::FilePath& puff_patch_path,
    const std::string& id,
    const std::string& fingerprint,
    std::unique_ptr<CrxInstaller::InstallParams> install_params,
    scoped_refptr<CrxInstaller> installer,
    std::unique_ptr<Unzipper> unzipper_,
    scoped_refptr<update_client::CrxCache> crx_cache,
    scoped_refptr<Patcher> patcher_,
    crx_file::VerifierFormat crx_format,
    CrxInstaller::ProgressCallback progress_callback,
    InstallOnBlockingTaskRunnerCompleteCallback callback,
    const CrxCache::Result& result) {
  if (result.error != UnpackerError::kNone) {
    main_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), ErrorCategory::kUnpack,
                       static_cast<int>(result.error), 0, std::nullopt));
    DVLOG(2) << "crx_cache->Get failed: " << static_cast<int>(result.error);
    return;
  }
  base::FilePath crx_path = result.crx_cache_path;
  base::FilePath dest_path = crx_path.DirName().AppendASCII(
      base::JoinString({"temp", fingerprint}, "_"));
  base::File crx_file(crx_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File puff_patch_file(puff_patch_path,
                             base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File dest_file(dest_path, base::File::FLAG_CREATE_ALWAYS |
                                      base::File::FLAG_WRITE |
                                      base::File::FLAG_WIN_EXCLUSIVE_WRITE);
  PuffinPatcher::Patch(
      std::move(crx_file), std::move(puff_patch_file), std::move(dest_file),
      patcher_,
      base::BindOnce(&OnPuffPatchCompleteOnBlockingTaskRunner, main_task_runner,
                     pk_hash, puff_patch_path, dest_path, id, fingerprint,
                     std::move(install_params), installer, std::move(unzipper_),
                     crx_cache, crx_format, progress_callback,
                     std::move(callback)));
}

void StartGetPreviousCrxOnBlockingTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    const std::vector<uint8_t>& pk_hash,
    const base::FilePath& puff_patch_path,
    const std::string& id,
    const std::string& previous_fingerprint,
    const std::string& fingerprint,
    std::unique_ptr<CrxInstaller::InstallParams> install_params,
    scoped_refptr<CrxInstaller> installer,
    std::unique_ptr<Unzipper> unzipper_,
    scoped_refptr<update_client::CrxCache> crx_cache,
    scoped_refptr<Patcher> patcher_,
    crx_file::VerifierFormat crx_format,
    CrxInstaller::ProgressCallback progress_callback,
    InstallOnBlockingTaskRunnerCompleteCallback callback) {
  main_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CrxCache::Get, crx_cache, id, previous_fingerprint,
          base::BindPostTask(
              base::ThreadPool::CreateSequencedTaskRunner(kTaskTraits),
              base::BindOnce(&StartPuffPatchOnBlockingTaskRunner,
                             main_task_runner, pk_hash, puff_patch_path, id,
                             fingerprint, std::move(install_params), installer,
                             std::move(unzipper_), crx_cache, patcher_,
                             crx_format, progress_callback,
                             std::move(callback)))));
}

// Returns a string literal corresponding to the value of the downloader |d|.
const char* DownloaderToString(CrxDownloader::DownloadMetrics::Downloader d) {
  switch (d) {
    case CrxDownloader::DownloadMetrics::kUrlFetcher:
      return "direct";
    case CrxDownloader::DownloadMetrics::kBits:
      return "bits";
    case CrxDownloader::DownloadMetrics::kBackgroundMac:
      return "nsurlsession_background";
    default:
      return "unknown";
  }
}

base::Value::Dict MakeEvent(
    UpdateClient::PingParams ping_params,
    const std::optional<base::Version>& previous_version,
    const std::optional<base::Version>& next_version) {
  base::Value::Dict event;
  event.Set("eventtype", ping_params.event_type);
  event.Set("eventresult", ping_params.result);
  if (ping_params.error_code) {
    event.Set("errorcode", ping_params.error_code);
  }
  if (ping_params.extra_code1) {
    event.Set("extracode1", ping_params.extra_code1);
  }
  if (!ping_params.app_command_id.empty()) {
    event.Set("appcommandid", ping_params.app_command_id);
  }
  if (previous_version) {
    event.Set("previousversion", previous_version->GetString());
  }
  if (next_version) {
    event.Set("nextversion", next_version->GetString());
  }
  return event;
}

}  // namespace

Component::Component(const UpdateContext& update_context, const std::string& id)
    : id_(id),
      state_(std::make_unique<StateNew>(this)),
      update_context_(update_context) {}

Component::~Component() = default;

scoped_refptr<Configurator> Component::config() const {
  return update_context_->config;
}

std::string Component::session_id() const {
  return update_context_->session_id;
}

bool Component::is_foreground() const {
  return update_context_->is_foreground;
}

void Component::Handle(CallbackHandleComplete callback_handle_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(state_);

  callback_handle_complete_ = std::move(callback_handle_complete);

  state_->Handle(
      base::BindOnce(&Component::ChangeState, base::Unretained(this)));
}

void Component::ChangeState(std::unique_ptr<State> next_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  previous_state_ = state();
  if (next_state) {
    state_ = std::move(next_state);
  } else {
    is_handled_ = true;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback_handle_complete_));
}

CrxUpdateItem Component::GetCrxUpdateItem() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CrxUpdateItem crx_update_item;
  crx_update_item.state = state_->state();
  crx_update_item.id = id_;
  if (crx_component_) {
    crx_update_item.component = *crx_component_;
  }
  crx_update_item.last_check = last_check_;
  crx_update_item.next_version = next_version_;
  crx_update_item.next_fp = next_fp_;
  crx_update_item.downloaded_bytes = downloaded_bytes_;
  crx_update_item.install_progress = install_progress_;
  crx_update_item.total_bytes = total_bytes_;
  crx_update_item.error_category = error_category_;
  crx_update_item.error_code = error_code_;
  crx_update_item.extra_code1 = extra_code1_;
  crx_update_item.custom_updatecheck_data = custom_attrs_;
  crx_update_item.installer_result = installer_result_;

  return crx_update_item;
}

void Component::SetParseResult(const ProtocolParser::Result& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK_EQ(0, update_check_error_);

  status_ = result.status;
  action_run_ = result.action_run;
  custom_attrs_ = result.custom_attributes;

  if (result.manifest.packages.empty()) {
    return;
  }

  next_version_ = base::Version(result.manifest.version);
  const auto& package = result.manifest.packages.front();
  next_fp_ = package.fingerprint;

  // Resolve the urls by combining the base urls with the package names.
  for (const auto& crx_url : result.crx_urls) {
    const GURL url = crx_url.Resolve(package.name);
    if (url.is_valid()) {
      crx_urls_.push_back(url);
    }
  }
  for (const auto& crx_diffurl : result.crx_diffurls) {
    const GURL url = crx_diffurl.Resolve(package.namediff);
    if (url.is_valid()) {
      crx_diffurls_.push_back(url);
    }
  }

  hash_sha256_ = package.hash_sha256;
  hashdiff_sha256_ = package.hashdiff_sha256;

  size_ = package.size;
  sizediff_ = package.sizediff;

  if (!result.manifest.run.empty()) {
    install_params_ = std::make_optional(CrxInstaller::InstallParams(
        result.manifest.run, result.manifest.arguments,
        [&result](const std::string& expected) -> std::string {
          if (expected.empty() || result.data.empty()) {
            return "";
          }

          const auto it = base::ranges::find(
              result.data, expected,
              &ProtocolParser::Result::Data::install_data_index);

          const bool matched = it != std::end(result.data);
          DVLOG(2) << "Expected install_data_index: " << expected
                   << ", matched: " << matched;

          return matched ? it->text : "";
        }(crx_component_ ? crx_component_->install_data_index : "")));
  }
}

void Component::PingOnly(const CrxComponent& crx_component,
                         UpdateClient::PingParams ping_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(ComponentState::kNew, state());
  crx_component_ = crx_component;
  previous_version_ = crx_component_->version;
  next_version_ = base::Version("0");
  error_code_ = ping_params.error_code;
  extra_code1_ = ping_params.extra_code1;
  state_ = std::make_unique<StatePingOnly>(this);
  AppendEvent(MakeEvent(ping_params, previous_version_, std::nullopt));
}

void Component::SetUpdateCheckResult(
    const std::optional<ProtocolParser::Result>& result,
    ErrorCategory error_category,
    int error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(ComponentState::kChecking, state());

  error_category_ = error_category;
  error_code_ = error;

  if (result) {
    SetParseResult(result.value());
  }
}

void Component::NotifyWait() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyObservers(Events::COMPONENT_WAIT);
}

bool Component::CanDoBackgroundDownload(int64_t size) const {
  // Foreground component updates are always downloaded in foreground.
  bool enabled =
      !is_foreground() &&
      update_context_->config->EnabledBackgroundDownloader();
#if BUILDFLAG(IS_MAC)
  enabled &=
      base::FeatureList::IsEnabled(features::kDynamicCrxDownloaderPriority) &&
      size > features::kDynamicCrxDownloaderPrioritySizeThreshold.Get();
#endif
  return enabled;
}

void Component::AppendEvent(base::Value::Dict event) {
  events_.push_back(std::move(event));
}

void Component::NotifyObservers(UpdateClient::Observer::Events event) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // There is no corresponding component state for the COMPONENT_WAIT event.
  if (update_context_->crx_state_change_callback &&
      event != UpdateClient::Observer::Events::COMPONENT_WAIT) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindRepeating(update_context_->crx_state_change_callback,
                            GetCrxUpdateItem()));
  }
  update_context_->notify_observers_callback.Run(event, id_);
}

base::TimeDelta Component::GetUpdateDuration() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (update_begin_.is_null()) {
    return base::TimeDelta();
  }
  const base::TimeDelta update_cost(base::TimeTicks::Now() - update_begin_);
  if (update_cost.is_negative()) {
    return base::TimeDelta();
  }
  return std::min(update_cost, update_context_->config->UpdateDelay());
}

base::Value::Dict Component::MakeEventUpdateComplete() const {
  base::Value::Dict event;
  event.Set("eventtype", update_context_->is_install
                             ? protocol_request::kEventInstall
                             : protocol_request::kEventUpdate);
  event.Set("eventresult",
            static_cast<int>(state() == ComponentState::kUpdated));
  if (error_category() != ErrorCategory::kNone) {
    event.Set("errorcat", static_cast<int>(error_category()));
  }
  if (error_code()) {
    event.Set("errorcode", error_code());
  }
  if (extra_code1()) {
    event.Set("extracode1", extra_code1());
  }
  if (HasDiffUpdate(*this)) {
    const int diffresult = static_cast<int>(!diff_update_failed());
    event.Set("diffresult", diffresult);
  }
  if (diff_error_category() != ErrorCategory::kNone) {
    const int differrorcat = static_cast<int>(diff_error_category());
    event.Set("differrorcat", differrorcat);
  }
  if (diff_error_code()) {
    event.Set("differrorcode", diff_error_code());
  }
  if (diff_extra_code1()) {
    event.Set("diffextracode1", diff_extra_code1());
  }
  if (!previous_fp().empty()) {
    event.Set("previousfp", previous_fp());
  }
  if (!next_fp().empty()) {
    event.Set("nextfp", next_fp());
  }
  CHECK(previous_version().IsValid());
  event.Set("previousversion", previous_version().GetString());
  if (next_version().IsValid()) {
    event.Set("nextversion", next_version().GetString());
  }
  return event;
}

base::Value::Dict Component::MakeEventDownloadMetrics(
    const CrxDownloader::DownloadMetrics& dm) const {
  base::Value::Dict event;
  event.Set("eventtype", protocol_request::kEventDownload);
  event.Set("eventresult", static_cast<int>(dm.error == 0));
  event.Set("downloader", DownloaderToString(dm.downloader));
  if (dm.error) {
    event.Set("errorcode", dm.error);
  }
  if (dm.extra_code1) {
    event.Set("extracode1", dm.extra_code1);
  }
  event.Set("url", dm.url.spec());

  // -1 means that the  byte counts are not known.
  if (dm.total_bytes != -1 &&
      dm.total_bytes < protocol_request::kProtocolMaxInt) {
    event.Set("total", static_cast<double>(dm.total_bytes));
  }
  if (dm.downloaded_bytes != -1 &&
      dm.total_bytes < protocol_request::kProtocolMaxInt) {
    event.Set("downloaded", static_cast<double>(dm.downloaded_bytes));
  }
  if (dm.download_time_ms &&
      dm.total_bytes < protocol_request::kProtocolMaxInt) {
    event.Set("download_time_ms", static_cast<double>(dm.download_time_ms));
  }
  CHECK(previous_version().IsValid());
  event.Set("previousversion", previous_version().GetString());
  if (next_version().IsValid()) {
    event.Set("nextversion", next_version().GetString());
  }
  return event;
}

base::Value::Dict Component::MakeEventActionRun(bool succeeded,
                                                int error_code,
                                                int extra_code1) const {
  base::Value::Dict event;
  event.Set("eventtype", protocol_request::kEventAction);
  event.Set("eventresult", static_cast<int>(succeeded));
  if (error_code) {
    event.Set("errorcode", error_code);
  }
  if (extra_code1) {
    event.Set("extracode1", extra_code1);
  }
  return event;
}

std::vector<base::Value::Dict> Component::GetEvents() const {
  std::vector<base::Value::Dict> events;
  for (const auto& event : events_) {
    events.push_back(event.Clone());
  }
  return events;
}

std::unique_ptr<CrxInstaller::InstallParams> Component::install_params() const {
  return install_params_
             ? std::make_unique<CrxInstaller::InstallParams>(*install_params_)
             : nullptr;
}

Component::State::State(Component* component, ComponentState state)
    : state_(state), component_(*component) {}

Component::State::~State() = default;

void Component::State::Handle(CallbackNextState callback_next_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  callback_next_state_ = std::move(callback_next_state);

  DoHandle();
}

void Component::State::TransitionState(std::unique_ptr<State> next_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(next_state);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_next_state_), std::move(next_state)));
}

void Component::State::EndState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_next_state_), nullptr));
}

Component::StateNew::StateNew(Component* component)
    : State(component, ComponentState::kNew) {}

Component::StateNew::~StateNew() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateNew::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();
  if (component.crx_component()) {
    TransitionState(std::make_unique<StateChecking>(&component));

    // Notify that the component is being checked for updates after the
    // transition to `StateChecking` occurs. This event indicates the start
    // of the update check. The component receives the update check results when
    // the update checks completes, and after that, `UpdateEngine` invokes the
    // function `StateChecking::DoHandle` to transition the component out of
    // the `StateChecking`. The current design allows for notifying observers
    // on state transitions but it does not allow such notifications when a
    // new state is entered. Hence, posting the task below is a workaround for
    // this design oversight.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](Component& component) {
              component.NotifyObservers(Events::COMPONENT_CHECKING_FOR_UPDATES);
            },
            std::ref(component)));
  } else {
    component.error_code_ = static_cast<int>(Error::CRX_NOT_FOUND);
    component.error_category_ = ErrorCategory::kService;
    TransitionState(std::make_unique<StateUpdateError>(&component));
  }
}

Component::StateChecking::StateChecking(Component* component)
    : State(component, ComponentState::kChecking) {
  component->last_check_ = base::TimeTicks::Now();
}

Component::StateChecking::~StateChecking() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateChecking::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();
  CHECK(component.crx_component());

  if (component.error_code_) {
    metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kError);
    TransitionState(std::make_unique<StateUpdateError>(&component));
    return;
  }

  if (component.update_context_->is_cancelled) {
    metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kCanceled);
    TransitionState(std::make_unique<StateUpdateError>(&component));
    component.error_category_ = ErrorCategory::kService;
    component.error_code_ = static_cast<int>(ServiceError::CANCELLED);
    return;
  }

  if (component.status_ == "ok") {
    metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kHasUpdate);
    TransitionState(std::make_unique<StateCanUpdate>(&component));
    return;
  }

  if (component.status_ == "noupdate") {
    metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kNoUpdate);
    if (component.action_run_.empty() ||
        component.update_context_->is_update_check_only) {
      TransitionState(std::make_unique<StateUpToDate>(&component));
    } else {
      TransitionState(std::make_unique<StateRun>(&component));
    }
    return;
  }

  metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kError);
  TransitionState(std::make_unique<StateUpdateError>(&component));
}

Component::StateUpdateError::StateUpdateError(Component* component)
    : State(component, ComponentState::kUpdateError) {}

Component::StateUpdateError::~StateUpdateError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateUpdateError::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();

  CHECK_NE(ErrorCategory::kNone, component.error_category_);
  CHECK_NE(0, component.error_code_);

  // Create an event only when the server response included an update.
  if (component.IsUpdateAvailable()) {
    component.AppendEvent(component.MakeEventUpdateComplete());
  }

  EndState();
  component.NotifyObservers(Events::COMPONENT_UPDATE_ERROR);
}

Component::StateCanUpdate::StateCanUpdate(Component* component)
    : State(component, ComponentState::kCanUpdate) {}

Component::StateCanUpdate::~StateCanUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateCanUpdate::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();
  CHECK(component.crx_component());

  component.is_update_available_ = true;
  component.NotifyObservers(Events::COMPONENT_UPDATE_FOUND);

  if (!component.crx_component()->updates_enabled ||
      (!component.crx_component()->allow_updates_on_metered_connection &&
       component.config()->IsConnectionMetered())) {
    component.error_category_ = ErrorCategory::kService;
    component.error_code_ = static_cast<int>(ServiceError::UPDATE_DISABLED);
    component.extra_code1_ = 0;
    metrics::RecordCanUpdateResult(metrics::CanUpdateResult::kUpdatesDisabled);
    TransitionState(std::make_unique<StateUpdateError>(&component));
    return;
  }

  if (component.update_context_->is_cancelled) {
    TransitionState(std::make_unique<StateUpdateError>(&component));
    component.error_category_ = ErrorCategory::kService;
    component.error_code_ = static_cast<int>(ServiceError::CANCELLED);
    metrics::RecordCanUpdateResult(metrics::CanUpdateResult::kCanceled);
    return;
  }

  if (component.update_context_->is_update_check_only) {
    component.error_category_ = ErrorCategory::kService;
    component.error_code_ =
        static_cast<int>(ServiceError::CHECK_FOR_UPDATE_ONLY);
    component.extra_code1_ = 0;
    component.AppendEvent(component.MakeEventUpdateComplete());
    EndState();
    metrics::RecordCanUpdateResult(
        metrics::CanUpdateResult::kCheckForUpdateOnly);
    return;
  }

  metrics::RecordCanUpdateResult(metrics::CanUpdateResult::kCanUpdate);

  // Start computing the cost of the this update from here on.
  component.update_begin_ = base::TimeTicks::Now();
  CHECK(component.update_context_->crx_cache_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &update_client::CrxCache::Get,
          component.update_context_->crx_cache_.value(),
          component.crx_component()->app_id, component.next_fp_,
          base::BindOnce(
              &Component::StateCanUpdate::GetNextCrxFromCacheComplete,
              base::Unretained(this))));
}

// Returns true if a differential update is available, it has not failed yet,
// and the configuration allows this update.
bool Component::StateCanUpdate::CanTryDiffUpdate() const {
  const auto& component = Component::State::component();
  return HasDiffUpdate(component) && !component.diff_error_code_ &&
         component.update_context_->crx_cache_.has_value() &&
         component.update_context_->config->EnabledDeltas();
}

void Component::StateCanUpdate::GetNextCrxFromCacheComplete(
    const CrxCache::Result& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto& component = State::component();
  if (result.error == UnpackerError::kNone) {
    component.payload_path_ = result.crx_cache_path;
    TransitionState(std::make_unique<StateUpdating>(&component));
    return;
  }
  if (CanTryDiffUpdate()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&update_client::CrxCache::Contains,
                       component.update_context_->crx_cache_.value(),
                       component.crx_component()->app_id,
                       component.previous_fp_),
        base::BindOnce(
            &Component::StateCanUpdate::CheckIfCacheContainsPreviousCrxComplete,
            base::Unretained(this)));
    return;
  }
  TransitionState(std::make_unique<StateDownloading>(&component));
}

void Component::StateCanUpdate::CheckIfCacheContainsPreviousCrxComplete(
    bool crx_is_in_cache) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto& component = State::component();
  if (crx_is_in_cache) {
    TransitionState(std::make_unique<StateDownloadingDiff>(&component));
  } else {
    // If the configuration allows diff update, but the previous crx
    // is not cached, report the kPuffinMissingPreviousCrx error.
    component.diff_error_category_ = ErrorCategory::kUnpack;
    component.diff_error_code_ =
        static_cast<int>(UnpackerError::kPuffinMissingPreviousCrx);
    TransitionState(std::make_unique<StateDownloading>(&component));
  }
}

Component::StateUpToDate::StateUpToDate(Component* component)
    : State(component, ComponentState::kUpToDate) {}

Component::StateUpToDate::~StateUpToDate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateUpToDate::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();
  CHECK(component.crx_component());

  component.NotifyObservers(Events::COMPONENT_ALREADY_UP_TO_DATE);
  EndState();
}

Component::StateDownloadingBase::StateDownloadingBase(Component* component,
                                                      ComponentState state)
    : State(component, state) {}

Component::StateDownloadingBase::~StateDownloadingBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateDownloadingBase::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();
  CHECK(component.crx_component());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(
          [](base::RepeatingCallback<int64_t(const base::FilePath&)>
                 get_available_space) -> int64_t {
            base::ScopedTempDir temp_dir;
            return temp_dir.CreateUniqueTempDir()
                       ? get_available_space.Run(temp_dir.GetPath())
                       : 0;
          },
          component.update_context_->get_available_space),
      base::BindOnce(&Component::StateDownloadingBase::HandleAvailableSpace,
                     base::Unretained(this)));
}

void Component::StateDownloadingBase::HandleAvailableSpace(
    const int64_t available_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();
  CHECK(component.crx_component());

  if (available_bytes <= component_size() * 2) {
    VLOG(1) << "available_bytes: " << available_bytes
            << ", download size: " << component_size();
    component.error_category_ = ErrorCategory::kDownload;
    component.error_code_ = static_cast<int>(CrxDownloaderError::DISK_FULL);

    TransitionState(std::make_unique<StateUpdateError>(&component));
    return;
  }

  component.downloaded_bytes_ = -1;
  component.total_bytes_ = -1;

  crx_downloader_ =
      component.config()->GetCrxDownloaderFactory()->MakeCrxDownloader(
          component.CanDoBackgroundDownload(component_size()));
  crx_downloader_->set_progress_callback(
      base::BindRepeating(&Component::StateDownloadingBase::DownloadProgress,
                          base::Unretained(this)));
  cancel_callback_ = crx_downloader_->StartDownload(
      component_crx_urls(), component_hash_sha256(),
      base::BindOnce(&Component::StateDownloadingBase::DownloadComplete,
                     base::Unretained(this)));

  component.NotifyObservers(Events::COMPONENT_UPDATE_DOWNLOADING);
}

// Called when progress is being made downloading a CRX. Can be called multiple
// times due to how the CRX downloader switches between different downloaders
// and fallback urls.
void Component::StateDownloadingBase::DownloadProgress(int64_t downloaded_bytes,
                                                       int64_t total_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto& component = Component::State::component();
  component.downloaded_bytes_ = downloaded_bytes;
  component.total_bytes_ = total_bytes;
  component.NotifyObservers(Events::COMPONENT_UPDATE_DOWNLOADING);
}

void Component::StateDownloadingBase::DownloadComplete(
    const CrxDownloader::Result& download_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();

  for (const auto& download_metrics : crx_downloader_->download_metrics()) {
    component.AppendEvent(component.MakeEventDownloadMetrics(download_metrics));
  }

  crx_downloader_ = nullptr;

  if (component.update_context_->is_cancelled) {
    TransitionState(std::make_unique<StateUpdateError>(&component));
    component.error_category_ = ErrorCategory::kService;
    component.error_code_ = static_cast<int>(ServiceError::CANCELLED);
    return;
  }

  if (download_result.error) {
    CHECK(download_result.response.empty());
    set_component_error_category(ErrorCategory::kDownload);
    set_component_error_code(download_result.error);

    TransitionState(next_state_on_error());
    return;
  }

  component.payload_path_ = download_result.response;

  TransitionState(next_state());
}

Component::StateDownloading::StateDownloading(Component* component)
    : StateDownloadingBase(component, ComponentState::kDownloading) {}
Component::StateDownloading::~StateDownloading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

int64_t Component::StateDownloading::component_size() const {
  return Component::State::component().size_;
}
std::vector<GURL> Component::StateDownloading::component_crx_urls() const {
  return Component::State::component().crx_urls_;
}
std::string Component::StateDownloading::component_hash_sha256() const {
  return Component::State::component().hash_sha256_;
}
void Component::StateDownloading::set_component_error_category(
    ErrorCategory error_category) {
  Component::State::component().error_category_ = error_category;
}
void Component::StateDownloading::set_component_error_code(int error_code) {
  Component::State::component().error_code_ = error_code;
}
std::unique_ptr<Component::State>
Component::StateDownloading::next_state_on_error() {
  auto& component = Component::State::component();
  return std::make_unique<Component::StateUpdateError>(&component);
}
std::unique_ptr<Component::State> Component::StateDownloading::next_state() {
  auto& component = Component::State::component();
  return std::make_unique<Component::StateUpdating>(&component);
}

Component::StateDownloadingDiff::StateDownloadingDiff(Component* component)
    : StateDownloadingBase(component, ComponentState::kDownloadingDiff) {}
Component::StateDownloadingDiff::~StateDownloadingDiff() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

int64_t Component::StateDownloadingDiff::component_size() const {
  return Component::State::component().sizediff_;
}
std::vector<GURL> Component::StateDownloadingDiff::component_crx_urls() const {
  return Component::State::component().crx_diffurls_;
}
std::string Component::StateDownloadingDiff::component_hash_sha256() const {
  return Component::State::component().hashdiff_sha256_;
}
void Component::StateDownloadingDiff::set_component_error_category(
    ErrorCategory error_category) {
  Component::State::component().diff_error_category_ = error_category;
}
void Component::StateDownloadingDiff::set_component_error_code(int error_code) {
  Component::State::component().diff_error_code_ = error_code;
}
std::unique_ptr<Component::State>
Component::StateDownloadingDiff::next_state_on_error() {
  auto& component = Component::State::component();
  return std::make_unique<Component::StateDownloading>(&component);
}
std::unique_ptr<Component::State>
Component::StateDownloadingDiff::next_state() {
  auto& component = Component::State::component();
  return std::make_unique<Component::StateUpdatingDiff>(&component);
}

Component::StateUpdatingDiff::StateUpdatingDiff(Component* component)
    : State(component, ComponentState::kUpdatingDiff) {}

Component::StateUpdatingDiff::~StateUpdatingDiff() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateUpdatingDiff::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();
  const auto& update_context = *component.update_context_;

  CHECK(component.crx_component());

  component.install_progress_ = -1;
  component.NotifyObservers(Events::COMPONENT_UPDATE_READY);

  // Adapts the repeating progress callback invoked by the installer so that
  // the callback can be posted to the main sequence instead of running
  // the callback on the sequence the installer is running on.
  auto main_task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  if (update_context.crx_cache_.has_value() &&
      update_context.config->EnabledDeltas()) {
    base::ThreadPool::CreateSequencedTaskRunner(kTaskTraits)
        ->PostTask(
            FROM_HERE,
            base::BindOnce(
                &update_client::StartGetPreviousCrxOnBlockingTaskRunner,
                base::SequencedTaskRunner::GetCurrentDefault(),
                component.crx_component()->pk_hash, component.payload_path_,
                component.crx_component()->app_id, component.previous_fp_,
                component.next_fp_, component.install_params(),
                component.crx_component()->installer,
                update_context.config->GetUnzipperFactory()->Create(),
                update_context.crx_cache_.value(),
                update_context.config->GetPatcherFactory()->Create(),
                component.crx_component()->crx_format_requirement,
                base::BindRepeating(
                    &Component::StateUpdatingDiff::InstallProgress,
                    base::Unretained(this)),
                base::BindOnce(&Component::StateUpdatingDiff::InstallComplete,
                               base::Unretained(this))));
  } else {
    // We shouldn't get here due to the check in CanTryDiffUpdate, but if we
    // do, return an error to avoid diff updates.
    main_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&Component::StateUpdatingDiff::InstallComplete,
                       base::Unretained(this), ErrorCategory::kUnpack,
                       static_cast<int>(UnpackerError::kCrxCacheNotProvided), 0,
                       std::nullopt));
  }
}

void Component::StateUpdatingDiff::InstallProgress(int install_progress) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();
  if (install_progress >= 0 && install_progress <= 100) {
    component.install_progress_ = install_progress;
  }
  component.NotifyObservers(Events::COMPONENT_UPDATE_UPDATING);
}

void Component::StateUpdatingDiff::InstallComplete(
    ErrorCategory error_category,
    int error_code,
    int extra_code1,
    std::optional<CrxInstaller::Result>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();

  component.diff_error_category_ = error_category;
  component.diff_error_code_ = error_code;
  component.diff_extra_code1_ = extra_code1;

  if (component.diff_error_code_ != 0) {
    TransitionState(std::make_unique<StateDownloading>(&component));
    return;
  }

  CHECK_EQ(ErrorCategory::kNone, component.diff_error_category_);
  CHECK_EQ(0, component.diff_error_code_);

  CHECK_EQ(ErrorCategory::kNone, component.error_category_);
  CHECK_EQ(0, component.error_code_);

  if (component.action_run_.empty()) {
    TransitionState(std::make_unique<StateUpdated>(&component));
  } else {
    TransitionState(std::make_unique<StateRun>(&component));
  }
}

Component::StateUpdating::StateUpdating(Component* component)
    : State(component, ComponentState::kUpdating) {}

Component::StateUpdating::~StateUpdating() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateUpdating::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();
  const auto& update_context = *component.update_context_;

  CHECK(component.crx_component());

  component.install_progress_ = -1;
  component.NotifyObservers(Events::COMPONENT_UPDATE_READY);

  // Adapts the repeating progress callback invoked by the installer so that
  // the callback can be posted to the main sequence instead of running
  // the callback on the sequence the installer is running on.
  auto main_task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  base::ThreadPool::CreateSequencedTaskRunner(kTaskTraits)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              &update_client::StartPuffinInstallOnBlockingTaskRunner,
              main_task_runner, component.crx_component()->pk_hash,
              component.payload_path_, component.crx_component()->app_id,
              component.next_fp_, component.install_params(),
              component.crx_component()->installer,
              update_context.config->GetUnzipperFactory()->Create(),
              component.crx_component()->allow_cached_copies &&
                      update_context.config->EnabledDeltas()
                  ? update_context.crx_cache_
                  : std::nullopt,
              component.crx_component()->crx_format_requirement,
              base::BindRepeating(&Component::StateUpdating::InstallProgress,
                                  base::Unretained(this)),
              base::BindOnce(&Component::StateUpdating::InstallComplete,
                             base::Unretained(this))));
}

void Component::StateUpdating::InstallProgress(int install_progress) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();
  if (install_progress >= 0 && install_progress <= 100) {
    component.install_progress_ = install_progress;
  }
  component.NotifyObservers(Events::COMPONENT_UPDATE_UPDATING);
}

void Component::StateUpdating::InstallComplete(
    ErrorCategory error_category,
    int error_code,
    int extra_code1,
    std::optional<CrxInstaller::Result> installer_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();

  component.error_category_ = error_category;
  component.error_code_ = error_code;
  component.extra_code1_ = extra_code1;
  component.installer_result_ = installer_result;

  CHECK(component.crx_component_);
  if (!component.crx_component_->allow_cached_copies &&
      component.update_context_->crx_cache_) {
    base::ThreadPool::CreateSequencedTaskRunner(kTaskTraits)
        ->PostTask(FROM_HERE,
                   base::BindOnce(&CrxCache::RemoveAll,
                                  component.update_context_->crx_cache_->get(),
                                  component.crx_component()->app_id));
  }

  if (component.error_code_ != 0) {
    TransitionState(std::make_unique<StateUpdateError>(&component));
    return;
  }

  CHECK_EQ(ErrorCategory::kNone, component.error_category_);
  CHECK_EQ(0, component.error_code_);

  if (component.action_run_.empty()) {
    TransitionState(std::make_unique<StateUpdated>(&component));
  } else {
    TransitionState(std::make_unique<StateRun>(&component));
  }
}

Component::StateUpdated::StateUpdated(Component* component)
    : State(component, ComponentState::kUpdated) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Component::StateUpdated::~StateUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateUpdated::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();
  CHECK(component.crx_component());

  component.crx_component_->version = component.next_version_;
  component.crx_component_->fingerprint = component.next_fp_;

  component.update_context_->persisted_data->SetProductVersion(
      component.id(), component.crx_component_->version);
  component.update_context_->persisted_data->SetMaxPreviousProductVersion(
      component.id(), component.previous_version_);
  component.update_context_->persisted_data->SetFingerprint(
      component.id(), component.crx_component_->fingerprint);

  component.AppendEvent(component.MakeEventUpdateComplete());

  component.NotifyObservers(Events::COMPONENT_UPDATED);
  metrics::RecordComponentUpdated();
  EndState();
}

Component::StatePingOnly::StatePingOnly(Component* component)
    : State(component, ComponentState::kPingOnly) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Component::StatePingOnly::~StatePingOnly() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StatePingOnly::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(State::component().crx_component());
  EndState();
}

Component::StateRun::StateRun(Component* component)
    : State(component, ComponentState::kRun) {}

Component::StateRun::~StateRun() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateRun::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& component = State::component();
  CHECK(component.crx_component());

  action_runner_ = std::make_unique<ActionRunner>(component);
  action_runner_->Run(
      base::BindOnce(&StateRun::ActionRunComplete, base::Unretained(this)));
}

void Component::StateRun::ActionRunComplete(bool succeeded,
                                            int error_code,
                                            int extra_code1) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();

  component.AppendEvent(
      component.MakeEventActionRun(succeeded, error_code, extra_code1));
  switch (component.previous_state_) {
    case ComponentState::kChecking:
      TransitionState(std::make_unique<StateUpToDate>(&component));
      return;
    case ComponentState::kUpdating:
    case ComponentState::kUpdatingDiff:
      TransitionState(std::make_unique<StateUpdated>(&component));
      return;
    default:
      break;
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace update_client
