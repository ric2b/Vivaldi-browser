// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef IOS_SYNC_FILE_STORE_FACTORY_H_
#define IOS_SYNC_FILE_STORE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace file_sync {
class SyncedFileStore;
}

class ProfileIOS;

class SyncedFileStoreFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static file_sync::SyncedFileStore* GetForProfile(ProfileIOS* profile);

  static SyncedFileStoreFactory* GetInstance();

  SyncedFileStoreFactory(const SyncedFileStoreFactory&) = delete;
  SyncedFileStoreFactory& operator=(const SyncedFileStoreFactory&) = delete;

 private:
  friend class base::NoDestructor<SyncedFileStoreFactory>;

  SyncedFileStoreFactory();
  ~SyncedFileStoreFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_SYNC_FILE_STORE_FACTORY_H_
