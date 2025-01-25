// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_SYNC_VIVALDI_INVALIDATION_SERVICE_FACTORY_H_
#define IOS_SYNC_VIVALDI_INVALIDATION_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "ios/chrome/browser/sync/model/sync_invalidations_service_factory.h"

class ProfileIOS;

namespace syncer {
class SyncInvalidationsService;
}  // namespace syncer

namespace vivaldi {

class VivaldiSyncInvalidationsServiceFactory
    : public SyncInvalidationsServiceFactory {
 public:
  VivaldiSyncInvalidationsServiceFactory(const VivaldiSyncInvalidationsServiceFactory&) =
      delete;
  VivaldiSyncInvalidationsServiceFactory& operator=(
      const VivaldiSyncInvalidationsServiceFactory&) = delete;

  static syncer::SyncInvalidationsService* GetForProfile(ProfileIOS* profile);

  static VivaldiSyncInvalidationsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<VivaldiSyncInvalidationsServiceFactory>;

  VivaldiSyncInvalidationsServiceFactory();
  ~VivaldiSyncInvalidationsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace vivaldi

#endif  // IOS_SYNC_VIVALDI_INVALIDATION_SERVICE_FACTORY_H_
