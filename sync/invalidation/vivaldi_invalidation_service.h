// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_INVALIDATION_VIVALDI_INVALIDATION_SERVICE_H_
#define SYNC_INVALIDATION_VIVALDI_INVALIDATION_SERVICE_H_

#include <memory>
#include <string>

#include "base/timer/timer.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/public/invalidation_service.h"
#include "net/base/backoff_entry.h"
#include "sync/invalidation/invalidation_service_stomp_websocket.h"
#include "vivaldi_account/vivaldi_account_manager.h"

class Profile;

namespace invalidation {
class InvalidationLogger;
}

namespace vivaldi {

class VivaldiInvalidationService
    : public invalidation::InvalidationService,
      public VivaldiAccountManager::Observer,
      public InvalidationServiceStompWebsocket::Client,
      public KeyedService {
 public:
  explicit VivaldiInvalidationService(Profile* profile);
  ~VivaldiInvalidationService() override;
  VivaldiInvalidationService(const VivaldiInvalidationService&) = delete;
  VivaldiInvalidationService& operator=(const VivaldiInvalidationService&) =
      delete;

  // Implementing InvalidationService
  void RegisterInvalidationHandler(
      invalidation::InvalidationHandler* handler) override;
  bool UpdateInterestedTopics(invalidation::InvalidationHandler* handler,
                              const invalidation::TopicSet& ids) override;
  void UnregisterInvalidationHandler(
      invalidation::InvalidationHandler* handler) override;
  invalidation::InvalidatorState GetInvalidatorState() const override;
  std::string GetInvalidatorClientId() const override;
  invalidation::InvalidationLogger* GetInvalidationLogger() override;
  void RequestDetailedStatus(base::Callback<void(const base::DictionaryValue&)>
                                 post_caller) const override;

  // Implementing VivaldiAccountManager::Observer
  void OnVivaldiAccountUpdated() override;
  void OnTokenFetchSucceeded() override;
  void OnVivaldiAccountShutdown() override;

  // Implementing InvalidationServiceStompWebsocket::Client
  std::string GetLogin() override;
  std::string GetVHost() override;
  std::string GetChannel() override;
  void OnConnected() override;
  void OnClosed() override;
  void OnInvalidation(base::Value) override;

 private:
  bool ConnectionAllowed();
  void ToggleConnectionIfNeeded();
  void SetUpConnection();
  void TearDownConnection();

  void PerformInvalidation(const invalidation::TopicInvalidationMap&);
  void UpdateInvalidatorState(invalidation::InvalidatorState state);

  Profile* profile_;
  VivaldiAccountManager* account_manager_;

  net::BackoffEntry websocket_backoff_;
  base::OneShotTimer websocket_backoff_timer_;

  std::string client_id_;
  std::unique_ptr<InvalidationServiceStompWebsocket> stomp_web_socket_;
  invalidation::InvalidatorRegistrarWithMemory invalidator_registrar_;
};

}  // namespace vivaldi

#endif  // SYNC_INVALIDATION_VIVALDI_INVALIDATION_SERVICE_H_
