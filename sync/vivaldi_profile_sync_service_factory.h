// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_PROFILE_SYNC_SERVICE_FACTORY_H_
#define SYNC_VIVALDI_PROFILE_SYNC_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace syncer {
class ProfileSyncService;
}

using syncer::ProfileSyncService;

namespace vivaldi {
class VivaldiProfileSyncService;

class VivaldiProfileSyncServiceFactory : public ProfileSyncServiceFactory {
 public:
  static ProfileSyncService* GetForProfile(Profile* profile);
  static VivaldiProfileSyncService* GetForProfileVivaldi(Profile* profile);
  static bool HasProfileSyncService(Profile* profile);

  static VivaldiProfileSyncServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<VivaldiProfileSyncServiceFactory>;

  VivaldiProfileSyncServiceFactory();
  ~VivaldiProfileSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace vivaldi

#endif  // SYNC_VIVALDI_PROFILE_SYNC_SERVICE_FACTORY_H_
