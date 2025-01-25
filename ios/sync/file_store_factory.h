// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef IOS_SYNC_FILE_STORE_FACTORY_H_
#define IOS_SYNC_FILE_STORE_FACTORY_H_

#include "base/no_destructor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace file_sync {
class SyncedFileStore;
}

class ProfileIOS;

class SyncedFileStoreFactory : public BrowserStateKeyedServiceFactory {
 public:
  static file_sync::SyncedFileStore* GetForProfile(ProfileIOS* browser_state);

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
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_SYNC_FILE_STORE_FACTORY_H_
