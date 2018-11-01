// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNCMANAGER_FACTORY_H_
#define SYNC_VIVALDI_SYNCMANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace browser_sync {
class ProfileSyncService;
}

using browser_sync::ProfileSyncService;

namespace vivaldi {
class VivaldiSyncManager;

class VivaldiSyncManagerFactory : public ProfileSyncServiceFactory {
 public:
  static ProfileSyncService* GetForProfile(Profile* profile);
  static VivaldiSyncManager* GetForProfileVivaldi(Profile* profile);
  static bool HasProfileSyncService(Profile* profile);

  static VivaldiSyncManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<VivaldiSyncManagerFactory>;

  VivaldiSyncManagerFactory();
  ~VivaldiSyncManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace vivaldi

#endif  // SYNC_VIVALDI_SYNCMANAGER_FACTORY_H_
