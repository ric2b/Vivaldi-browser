// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_SYNC_VIVALDI_SYNC_SERVICE_FACTORY_H_
#define IOS_SYNC_VIVALDI_SYNC_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"

class ChromeBrowserState;

namespace syncer {
class SyncServiceImpl;
class SyncService;
}  // namespace syncer

namespace vivaldi {
class VivaldiSyncServiceImpl;
// Singleton that owns all SyncServices and associates them with
// ChromeBrowserState.
class VivaldiSyncServiceFactory : public SyncServiceFactory {
 public:
  static VivaldiSyncServiceFactory* GetInstance();

  static VivaldiSyncServiceImpl* GetForBrowserStateVivaldi(
      ChromeBrowserState* browser_state);

 private:
  friend class base::NoDestructor<VivaldiSyncServiceFactory>;

  VivaldiSyncServiceFactory();
  ~VivaldiSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};
}  // namespace vivaldi

#endif  // IOS_SYNC_VIVALDI_SYNC_SERVICE_FACTORY_H_
