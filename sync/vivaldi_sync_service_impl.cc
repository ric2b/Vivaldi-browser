// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_service_impl.h"

#include <utility>

#include "app/vivaldi_apptools.h"
#include "base/task/post_task.h"
#include "base/version.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/sync_util.h"
#include "components/sync/engine/net/url_translator.h"
#include "components/sync/protocol/sync.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "sync/vivaldi_sync_auth_manager.h"

namespace vivaldi {

VivaldiSyncServiceImpl::VivaldiSyncServiceImpl(
    SyncServiceImpl::InitParams* init_params,
    Profile* profile,
    VivaldiAccountManager* account_manager)
    : SyncServiceImpl(std::move(*init_params)),
      profile_(profile),
      ui_helper_(profile, this),
      weak_factory_(this) {
  if (!vivaldi::ForcedVivaldiRunning()) {
    auth_manager_ = std::make_unique<VivaldiSyncAuthManager>(
        identity_manager_,
        base::BindRepeating(&VivaldiSyncServiceImpl::AccountStateChanged,
                            base::Unretained(this)),
        base::BindRepeating(&VivaldiSyncServiceImpl::CredentialsChanged,
                            base::Unretained(this)),
        account_manager);
  }

  // Notes must be re-synchronized to correct the note-duplication issues.
  base::Version last_seen_version(
      profile_->GetPrefs()->GetString(vivaldiprefs::kStartupLastSeenVersion));

  base::Version up_to_date_version(std::vector<uint32_t>({2, 8, 0, 0}));

  if (last_seen_version.IsValid() && last_seen_version < up_to_date_version)
    force_local_data_reset_ = true;
}

VivaldiSyncServiceImpl::~VivaldiSyncServiceImpl() {}

void VivaldiSyncServiceImpl::Initialize() {
  SyncServiceImpl::Initialize();
  ui_helper_.RegisterObserver();
}

void VivaldiSyncServiceImpl::ClearSyncData() {
  // This isn't handled by the engine anymore, so we instead do the whole
  // request right here and shut down sync.

  std::string client_id = engine_->GetCacheGuid();
  std::string auth_token = auth_manager_->GetCredentials().access_token;
  is_clearing_sync_data_ = true;
  StopAndClear();

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

  auto url_loader_factory = profile_->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();

  clear_data_url_loader_->DownloadHeadersOnly(
      url_loader_factory.get(),
      base::BindOnce(&VivaldiSyncServiceImpl::OnClearDataComplete,
                     base::Unretained(this)));

  NotifyObservers();
}

void VivaldiSyncServiceImpl::OnEngineInitialized(
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    bool success,
    bool is_first_time_sync_configure) {
  SyncServiceImpl::OnEngineInitialized(debug_info_listener, success,
                                       is_first_time_sync_configure);

  if (!force_local_data_reset_)
    return;
  force_local_data_reset_ = false;
  syncer::SyncProtocolError error;
  error.error_type = syncer::CLIENT_DATA_OBSOLETE;
  error.action = syncer::RESET_LOCAL_SYNC_DATA;

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&SyncServiceImpl::OnActionableError,
                                base::Unretained(this), error));
}

void VivaldiSyncServiceImpl::ResetEngine(syncer::ShutdownReason reason,
                                         ResetEngineReason reset_reason) {
  if (reason == syncer::ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA) {
    sync_client_->GetPrefService()->ClearPref(
        vivaldiprefs::kSyncIsUsingSeparateEncryptionPassword);
  }
  SyncServiceImpl::ResetEngine(reason, reset_reason);
}

void VivaldiSyncServiceImpl::OnClearDataComplete(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  is_clearing_sync_data_ = false;
  NotifyObservers();
}

std::string VivaldiSyncServiceImpl::GetEncryptionBootstrapToken() const {
  return sync_prefs_.GetEncryptionBootstrapToken();
}

void VivaldiSyncServiceImpl::SetEncryptionBootstrapToken(
    const std::string& token) {
  StopAndClear();
  sync_prefs_.SetEncryptionBootstrapToken(token);
  GetUserSettings()->SetSyncRequested(true);
}
}  // namespace vivaldi
