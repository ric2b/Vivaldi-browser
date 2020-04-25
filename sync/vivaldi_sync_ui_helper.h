// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNC_UI_HELPER_H_
#define SYNC_VIVALDI_SYNC_UI_HELPER_H_

#include "chrome/browser/profiles/profile.h"
#include "components/sync/driver/sync_service_observer.h"

namespace vivaldi {

class VivaldiProfileSyncService;

class VivaldiSyncUIHelper : public syncer::SyncServiceObserver {
 public:
  VivaldiSyncUIHelper(Profile* profile, VivaldiProfileSyncService* sync_manager);
  ~VivaldiSyncUIHelper() override;

  bool SetEncryptionPassword(const std::string& password);

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  Profile* profile_;
  VivaldiProfileSyncService* sync_manager_;

  bool tried_decrypt_ = false;
};
}  // namespace vivaldi

#endif  // SYNC_VIVALDI_SYNC_UI_HELPER_H_