// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_SYNC_NOTE_SYNC_SERVICE_FACTORY_H_
#define IOS_SYNC_NOTE_SYNC_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace sync_notes {
class NoteSyncService;
}

namespace vivaldi {
// Singleton that owns the note sync service.
class NoteSyncServiceFactory : public ProfileKeyedServiceFactoryIOS {
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
};

}  // namespace vivaldi

#endif  // IOS_SYNC_NOTE_SYNC_SERVICE_FACTORY_H_
