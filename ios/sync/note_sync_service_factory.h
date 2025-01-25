// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_SYNC_NOTE_SYNC_SERVICE_FACTORY_H_
#define IOS_SYNC_NOTE_SYNC_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ProfileIOS;

namespace sync_notes {
class NoteSyncService;
}

namespace vivaldi {
// Singleton that owns the note sync service.
class NoteSyncServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the instance of NoteSyncService associated with this profile
  // (creating one if none exists).
  static sync_notes::NoteSyncService* GetForProfile(ProfileIOS* profile);

  // Returns an instance of the NoteSyncServiceFactory singleton.
  static NoteSyncServiceFactory* GetInstance();

  NoteSyncServiceFactory(const NoteSyncServiceFactory&) = delete;
  NoteSyncServiceFactory& operator=(const NoteSyncServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<NoteSyncServiceFactory>;

  NoteSyncServiceFactory();
  ~NoteSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace vivaldi

#endif  // IOS_SYNC_NOTE_SYNC_SERVICE_FACTORY_H_
