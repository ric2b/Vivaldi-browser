// Copyright (c) 2015-2020 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNC_SERVICE_FACTORY_H_
#define SYNC_VIVALDI_SYNC_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace syncer {
class SyncService;
}

using syncer::SyncService;

namespace vivaldi {
class VivaldiSyncServiceImpl;

class VivaldiSyncServiceFactory : public SyncServiceFactory {
 public:
  static SyncService* GetForProfile(Profile* profile);
  static VivaldiSyncServiceImpl* GetForProfileVivaldi(Profile* profile);
  static bool HasSyncService(Profile* profile);

  static VivaldiSyncServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<VivaldiSyncServiceFactory>;

  VivaldiSyncServiceFactory();
  ~VivaldiSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace vivaldi

#endif  // SYNC_VIVALDI_SYNC_SERVICE_FACTORY_H_
