// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef SYNC_FILE_SYNC_FILE_STORE_FACTORY_H_
#define SYNC_FILE_SYNC_FILE_STORE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace file_sync {
class SyncedFileStore;
}

class SyncedFileStoreFactory : public BrowserContextKeyedServiceFactory {
 public:
  static file_sync::SyncedFileStore* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static SyncedFileStoreFactory* GetInstance();

  SyncedFileStoreFactory(const SyncedFileStoreFactory&) = delete;
  SyncedFileStoreFactory& operator=(const SyncedFileStoreFactory&) = delete;

 private:
  friend class base::NoDestructor<SyncedFileStoreFactory>;

  SyncedFileStoreFactory();
  ~SyncedFileStoreFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // SYNC_FILE_SYNC_FILE_STORE_FACTORY_H_
