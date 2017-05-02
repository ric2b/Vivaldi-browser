// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNC_MODEL_OBSERVER_H
#define SYNC_VIVALDI_SYNC_MODEL_OBSERVER_H

class VivaldiSyncModel;

namespace vivaldi {

// Observer for the SyncModel
class VivaldiSyncModelObserver {
 public:
  virtual void SyncOnMessage(const std::string& param1,
                             const std::string& param2) = 0;
  virtual void SyncModelBeingDeleted(VivaldiSyncModel* model) = 0;

 protected:
  virtual ~VivaldiSyncModelObserver() {}
};

}  // namespace vivaldi

#endif  // SYNC_VIVALDI_SYNC_MODEL_OBSERVER_H
