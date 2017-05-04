// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_SYNC_MODEL_H
#define VIVALDI_SYNC_MODEL_H

#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "components/keyed_service/core/keyed_service.h"

namespace vivaldi {
class VivaldiSyncManager;
class VivaldiSyncModelObserver;
}

// VivaldiSyncModel -----------------------------------------------------------
class VivaldiSyncModel : public KeyedService {
 public:
  explicit VivaldiSyncModel(vivaldi::VivaldiSyncManager* client);

  ~VivaldiSyncModel() override;

  // KeyedService:
  void Shutdown() override;

  void AddObserver(vivaldi::VivaldiSyncModelObserver* observer);
  void RemoveObserver(vivaldi::VivaldiSyncModelObserver* observer);

  void newMessage(const std::string& param1, const std::string& param2);

  // The observers.
  base::ObserverList<vivaldi::VivaldiSyncModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiSyncModel);
};

#endif  // VIVALDI_SYNC_MODEL_H
