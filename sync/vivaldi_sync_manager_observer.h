// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNC_MANAGER_OBSERVER_H_
#define SYNC_VIVALDI_SYNC_MANAGER_OBSERVER_H_

namespace vivaldi {

// Observer for the SyncModel
class VivaldiSyncManagerObserver {
 public:
  virtual void OnAccessTokenRequested() {}
  virtual void OnEncryptionPasswordRequested() {}
  virtual void OnLoginDone() {}
  virtual void OnSyncEngineInitFailed() {}
  virtual void OnBeginSyncing() {}
  virtual void OnEndSyncing() {}
  virtual void OnLogoutDone() {}

  virtual void OnDeletingSyncManager() = 0;

 protected:
  virtual ~VivaldiSyncManagerObserver() {}
};

}  // namespace vivaldi

#endif  // SYNC_VIVALDI_SYNC_MANAGER_OBSERVER_H_
