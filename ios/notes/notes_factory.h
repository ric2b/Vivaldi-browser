// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_NOTES_NOTES_FACTORY_H_
#define IOS_NOTES_NOTES_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace vivaldi {

class NotesModel;

class NotesModelFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static NotesModel* GetForProfile(ProfileIOS* profile);
  static NotesModel* GetForProfileIfExists(ProfileIOS* profile);
  static NotesModelFactory* GetInstance();

 private:
  friend class base::NoDestructor<NotesModelFactory>;

  NotesModelFactory();
  ~NotesModelFactory() override;
  NotesModelFactory(const NotesModelFactory&) = delete;
  NotesModelFactory& operator=(const NotesModelFactory&) = delete;

  // BrowserStateKeyedServiceFactory implementation.
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace vivaldi

#endif  // IOS_NOTES_NOTES_FACTORY_H_
