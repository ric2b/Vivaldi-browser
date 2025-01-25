// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_INVALIDATION_VIVALDI_INVALIDATION_SERVICE_H_
#define SYNC_INVALIDATION_VIVALDI_INVALIDATION_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/timer/timer.h"
#include "components/sync/base/data_type.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "net/base/backoff_entry.h"
#include "sync/invalidations/invalidation_service_stomp_client.h"
#include "vivaldi_account/vivaldi_account_manager.h"

class PrefService;

namespace invalidation {
class InvalidationLogger;
}

namespace vivaldi {

using NetworkContextProvider =
    base::RepeatingCallback<network::mojom::NetworkContext*()>;

class VivaldiSyncInvalidationsService
    : public syncer::SyncInvalidationsService,
      public VivaldiAccountManager::Observer,
      public InvalidationServiceStompClient::Delegate {
 public:
  VivaldiSyncInvalidationsService(
      const std::string& notification_server_url,
      VivaldiAccountManager* account_manager,
      NetworkContextProvider network_context_provider);
  ~VivaldiSyncInvalidationsService() override;
  VivaldiSyncInvalidationsService(const VivaldiSyncInvalidationsService&) =
      delete;
  VivaldiSyncInvalidationsService& operator=(
      const VivaldiSyncInvalidationsService&) = delete;

  void StartListening() override;
  void StopListening() override;
  void StopListeningPermanently() override;
  void AddListener(syncer::InvalidationsListener* listener) override;
  bool HasListener(syncer::InvalidationsListener* listener) override;
  void RemoveListener(syncer::InvalidationsListener* listener) override;
  void AddTokenObserver(
      syncer::FCMRegistrationTokenObserver* observer) override;
  void RemoveTokenObserver(
      syncer::FCMRegistrationTokenObserver* observer) override;
  std::optional<std::string> GetFCMRegistrationToken() const override;
  void SetInterestedDataTypesHandler(
      syncer::InterestedDataTypesHandler* handler) override;
  std::optional<syncer::DataTypeSet> GetInterestedDataTypes() const override;
  void SetInterestedDataTypes(const syncer::DataTypeSet& data_types) override;
  void SetCommittedAdditionalInterestedDataTypesCallback(
      InterestedDataTypesAppliedCallback callback) override;

  // Implementing VivaldiAccountManager::Observer
  void OnVivaldiAccountUpdated() override;
  void OnTokenFetchSucceeded() override;
  void OnVivaldiAccountShutdown() override;

  // Implementing InvalidationServiceStompClient::Delegate
  std::string GetLogin() const override;
  std::string GetVHost() const override;
  std::string GetChannel() const override;
  void OnConnected() override;
  void OnClosed() override;
  void OnInvalidation(std::string message) override;

 private:
  bool ConnectionAllowed() const;
  void ToggleConnectionIfNeeded();

  GURL notification_server_url_;
  VivaldiAccountManager* account_manager_;
  NetworkContextProvider network_context_provider_;

  net::BackoffEntry stomp_client_backoff_;
  base::OneShotTimer stomp_client_backoff_timer_;

  bool invalidations_requested_ = false;

  // A list of the latest incoming messages, used to replay incoming messages
  // whenever a new listener is added.
  base::circular_deque<std::string> last_received_messages_;

  raw_ptr<syncer::InterestedDataTypesHandler> interested_data_types_handler_ =
      nullptr;
  std::optional<syncer::DataTypeSet> interested_data_types_;

  base::ObserverList<syncer::InvalidationsListener,
                     /*check_empty=*/true,
                     /*allow_reentrancy=*/false>
      listeners_;

  // Contains all FCM token observers to notify about each token change.
  base::ObserverList<syncer::FCMRegistrationTokenObserver,
                     /*check_empty=*/true,
                     /*allow_reentrancy=*/false>
      token_observers_;

  std::unique_ptr<InvalidationServiceStompClient> stomp_client_;
};

}  // namespace vivaldi

#endif  // SYNC_INVALIDATION_VIVALDI_INVALIDATION_SERVICE_H_
