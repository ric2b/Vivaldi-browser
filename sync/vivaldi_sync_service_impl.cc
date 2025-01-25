// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_service_impl.h"

#include <utility>

#include "app/vivaldi_apptools.h"
#include "base/version.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/sync_util.h"
#include "components/sync/engine/net/url_translator.h"
#include "components/sync/protocol/sync.pb.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "sync/vivaldi_sync_auth_manager.h"
#include "vivaldi_account/vivaldi_account_manager.h"

namespace vivaldi {
namespace {
class TryAccountPasswordForDecryption : public syncer::SyncServiceObserver {
 public:
  TryAccountPasswordForDecryption(syncer::SyncService* sync_service,
                                  VivaldiAccountManager* account_manager)
      : account_manager_(account_manager) {
    sync_service->AddObserver(this);
  }
  ~TryAccountPasswordForDecryption() override = default;

  TryAccountPasswordForDecryption(
      const TryAccountPasswordForDecryption& ui_helper) = delete;
  TryAccountPasswordForDecryption& operator=(
      const TryAccountPasswordForDecryption& ui_helper) = delete;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override {
    if (!sync->IsEngineInitialized()) {
      tried_decrypt_ = false;
      return;
    }

    if (!sync->GetUserSettings()->IsPassphraseRequiredForPreferredDataTypes() ||
        tried_decrypt_)
      return;

    tried_decrypt_ = true;

    std::string password = account_manager_->password_handler()->password();

    if (!password.empty()) {
      // See if the user is using the same encryption and login password. If
      // yes, this will cause the engine to proceed to the next step, and cause
      // the encryption password prompt UI to be skipped. Otherwise, the UI will
      // just stick to showing the password prompt, so we can silently drop
      // informing the UI about it.
      std::ignore = sync->GetUserSettings()->SetDecryptionPassphrase(password);
    };
  }

  void OnSyncShutdown(syncer::SyncService* sync) override {
    sync->RemoveObserver(this);
    delete this;
  }

 private:
  const raw_ptr<VivaldiAccountManager> account_manager_;

  bool tried_decrypt_ = false;
};
}  // namespace

VivaldiSyncServiceImpl::VivaldiSyncServiceImpl(
    SyncServiceImpl::InitParams init_params,
    PrefService* prefs,
    VivaldiAccountManager* account_manager)
    : SyncServiceImpl(std::move(init_params)), weak_factory_(this) {
  if (vivaldi::IsVivaldiRunning()) {
    auth_manager_ = std::make_unique<VivaldiSyncAuthManager>(
        identity_manager_,
        base::BindRepeating(&VivaldiSyncServiceImpl::AccountStateChanged,
                            base::Unretained(this)),
        base::BindRepeating(&VivaldiSyncServiceImpl::CredentialsChanged,
                            base::Unretained(this)),
        account_manager);
  }

  // Self-destructs when sync shuts down.
  new TryAccountPasswordForDecryption(this, account_manager);

  // Notes must be re-synchronized to correct the note-duplication issues.
  base::Version last_seen_version(
      prefs->GetString(vivaldiprefs::kStartupLastSeenVersion));

  base::Version up_to_date_version(std::vector<uint32_t>({2, 8, 0, 0}));

  if (last_seen_version.IsValid() && last_seen_version < up_to_date_version)
    force_local_data_reset_ = true;
}

VivaldiSyncServiceImpl::~VivaldiSyncServiceImpl() {}

void VivaldiSyncServiceImpl::ClearSyncData() {
  // This isn't handled by the engine anymore, so we instead do the whole
  // request right here and shut down sync.

  std::string client_id = engine_->GetCacheGuid();
  std::string auth_token = auth_manager_->GetCredentials().access_token;
  is_clearing_sync_data_ = true;
  StopAndClear(ResetEngineReason::kResetLocalData);

  sync_pb::ClientToServerMessage request;
  request.set_share(auth_manager_->GetCredentials().email);
  request.set_message_contents(
      sync_pb::ClientToServerMessage::CLEAR_SERVER_DATA);
  request.mutable_clear_server_data();
  std::string request_content;
  request.SerializeToString(&request_content);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("sync_http_bridge", R"(
        semantics {
          sender: "Chrome Sync"
          description:
            "Chrome Sync synchronizes profile data between Chromium clients "
            "and Google for a given user account."
          trigger:
            "User makes a change to syncable profile data after enabling sync "
            "on the device."
          data:
            "The device and user identifiers, along with any profile data that "
            "is changing."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can disable Chrome Sync by going into the profile settings "
            "and choosing to Sign Out."
          chrome_policy {
            SyncDisabled {
              policy_options {mode: MANDATORY}
              SyncDisabled: true
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  std::string full_path = sync_service_url_.path() + "/command/";
  GURL::Replacements path_replacement;
  path_replacement.SetPathStr(full_path);

  resource_request->url = syncer::AppendSyncQueryString(
      sync_service_url_.ReplaceComponents(path_replacement), client_id);
  resource_request->method = "POST";
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  resource_request->headers.AddHeadersFromString(
      std::string("Authorization: Bearer ") + auth_token);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                      syncer::MakeUserAgentForSync(channel_));
  clear_data_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  clear_data_url_loader_->AttachStringForUpload(request_content,
                                                "application/octet-stream");

  clear_data_url_loader_->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindOnce(&VivaldiSyncServiceImpl::OnClearDataComplete,
                     base::Unretained(this)));

  NotifyObservers();
}

void VivaldiSyncServiceImpl::OnEngineInitialized(
    bool success,
    bool is_first_time_sync_configure) {
  SyncServiceImpl::OnEngineInitialized(success, is_first_time_sync_configure);

  if (!force_local_data_reset_)
    return;
  force_local_data_reset_ = false;
  syncer::SyncProtocolError error;
  error.error_type = syncer::CLIENT_DATA_OBSOLETE;
  error.action = syncer::RESET_LOCAL_SYNC_DATA;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SyncServiceImpl::OnActionableProtocolError,
                                weak_factory_.GetWeakPtr(), error));
}

void VivaldiSyncServiceImpl::OnClearDataComplete(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  is_clearing_sync_data_ = false;
  NotifyObservers();
}

std::string VivaldiSyncServiceImpl::GetEncryptionBootstrapTokenForBackup() {
  return GetEncryptionBootstrapToken();
}

void VivaldiSyncServiceImpl::ResetEncryptionBootstrapTokenFromBackup(
    const std::string& token) {
  StopAndClear(ResetEngineReason::kCredentialsChanged);
  SetEncryptionBootstrapToken(token);
  SetSyncFeatureRequested();
}
}  // namespace vivaldi
