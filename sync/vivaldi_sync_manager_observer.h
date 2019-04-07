// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNC_MANAGER_OBSERVER_H_
#define SYNC_VIVALDI_SYNC_MANAGER_OBSERVER_H_

#include "base/observer_list_types.h"

namespace vivaldi {

// Observer for the SyncModel
class VivaldiSyncManagerObserver : public base::CheckedObserver {
 public:
  virtual void OnAccessTokenRequested() {}
  virtual void OnEncryptionPasswordRequested() {}
  virtual void OnEngineStarted() {}
  virtual void OnEngineInitFailed() {}
  virtual void OnBeginSyncing() {}
  virtual void OnEndSyncing() {}
  virtual void OnEngineStopped() {}

  virtual void OnDeletingSyncManager() = 0;

 protected:
  ~VivaldiSyncManagerObserver() override {}
};

}  // namespace vivaldi

#endif  // SYNC_VIVALDI_SYNC_MANAGER_OBSERVER_H_
