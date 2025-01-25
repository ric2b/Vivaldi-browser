// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_NOTES_NOTES_FACTORY_H_
#define IOS_NOTES_NOTES_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ProfileIOS;

namespace vivaldi {

class NotesModel;

class NotesModelFactory : public BrowserStateKeyedServiceFactory {
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
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace vivaldi

#endif  // IOS_NOTES_NOTES_FACTORY_H_
