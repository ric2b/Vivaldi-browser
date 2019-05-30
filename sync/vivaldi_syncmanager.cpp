// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_syncmanager.h"

#include <utility>

#include "components/browser_sync/sync_auth_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sync/device_info/device_info_sync_service.h"
#include "components/sync/device_info/local_device_info_provider.h"
#include "components/sync/engine_impl/net/url_translator.h"
#include "components/sync/protocol/sync.pb.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

#include "prefs/vivaldi_gen_prefs.h"
#include "sync/vivaldi_sync_auth_manager.h"

namespace vivaldi {

VivaldiSyncManager::VivaldiSyncManager(
    ProfileSyncService::InitParams* init_params,
    Profile* profile,
    std::shared_ptr<VivaldiInvalidationService> invalidation_service,
    VivaldiAccountManager* account_manager)
    : ProfileSyncService(std::move(*init_params)),
      profile_(profile),
      invalidation_service_(invalidation_service),
      ui_helper_(profile, this),
      weak_factory_(this) {
  auth_manager_ = std::make_unique<VivaldiSyncAuthManager>(
      identity_manager_,
      base::BindRepeating(&VivaldiSyncManager::AccountStateChanged,
                          base::Unretained(this)),
      base::BindRepeating(&VivaldiSyncManager::CredentialsChanged,
                          base::Unretained(this)),
      account_manager);
}

VivaldiSyncManager::~VivaldiSyncManager() {}

void VivaldiSyncManager::ClearSyncData() {
  // This isn't handled by the engine anymore, so we instead do the whole
  // request right here and shut down sync.

  std::string client_id = sync_prefs_.GetCacheGuid();
  std::string auth_token = auth_manager_->GetCredentials().sync_token;
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
  GURL::Replacements replacements;
  std::string new_path = sync_service_url_.path() + "/command/";
  std::string new_query = syncer::MakeSyncQueryString(client_id);
  replacements.SetPath(new_path.c_str(), url::Component(0, new_path.length()));
  replacements.SetQuery(new_query.c_str(),
                        url::Component(0, new_query.length()));

  resource_request->url = sync_service_url_.ReplaceComponents(replacements);
  resource_request->method = "POST";
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES;

  resource_request->headers.AddHeadersFromString(
      std::string("Authorization: Bearer ") + auth_token);

  resource_request->headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                      sync_client_->GetDeviceInfoSyncService()
                                          ->GetLocalDeviceInfoProvider()
                                          ->GetSyncUserAgent());

  clear_data_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  clear_data_url_loader_->AttachStringForUpload(request_content,
                                                "application/octet-stream");

  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess();

  clear_data_url_loader_->DownloadHeadersOnly(
      url_loader_factory.get(),
      base::BindOnce(&VivaldiSyncManager::OnClearDataComplete,
                     base::Unretained(this)));

  NotifyObservers();
}

void VivaldiSyncManager::OnEngineInitialized(
    syncer::ModelTypeSet initial_types,
    const syncer::WeakHandle<syncer::JsBackend>& js_backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    const std::string& cache_guid,
    const std::string& session_name,
    const std::string& birthday,
    const std::string& bag_of_chips,
    bool success) {
  std::string custom_session_name =
      sync_client_->GetPrefService()->GetString(vivaldiprefs::kSyncSessionName);
  ProfileSyncService::OnEngineInitialized(
      initial_types, js_backend, debug_info_listener, cache_guid,
      custom_session_name.empty() ? session_name : custom_session_name,
      birthday, bag_of_chips, success);
}

void VivaldiSyncManager::StartSyncingWithServer() {
  // It is possible to cause sync to start without encryption turned on
  // by clicking "Request Start" in vivaldi://sync-internals. We prevent that
  // here.
  if (user_settings_->IsEncryptEverythingEnabled()) {
    ProfileSyncService::StartSyncingWithServer();
  }
}

void VivaldiSyncManager::ShutdownImpl(syncer::ShutdownReason reason) {
  if (reason == syncer::DISABLE_SYNC) {
    sync_client_->GetPrefService()->ClearPref(
        vivaldiprefs::kSyncIsUsingSeparateEncryptionPassword);
  }
  ProfileSyncService::ShutdownImpl(reason);
}

void VivaldiSyncManager::OnClearDataComplete(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  is_clearing_sync_data_ = false;
  NotifyObservers();
}
}  // namespace vivaldi
