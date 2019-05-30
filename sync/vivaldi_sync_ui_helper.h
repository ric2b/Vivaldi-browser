// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNC_UI_HELPER_H_
#define SYNC_VIVALDI_SYNC_UI_HELPER_H_

#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/sync/driver/sync_service_observer.h"

namespace vivaldi {

class VivaldiSyncManager;

class VivaldiSyncUIHelper : public syncer::SyncServiceObserver,
                            public password_manager::PasswordStoreConsumer {
 public:
  VivaldiSyncUIHelper(Profile* profile, VivaldiSyncManager* sync_manager);
  ~VivaldiSyncUIHelper() override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // Implementing password_manager::PasswordStoreConsumer
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

 private:
  Profile* profile_;
  VivaldiSyncManager* sync_manager_;

  bool tried_decrypt_ = false;
};
}  // namespace vivaldi

#endif  // SYNC_VIVALDI_SYNC_UI_HELPER_H_