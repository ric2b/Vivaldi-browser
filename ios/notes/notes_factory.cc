// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/notes/notes_factory.h"

#import "base/no_destructor.h"
#import "components/notes/note_node.h"
#import "components/notes/notes_model.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/sync/file_store_factory.h"
#import "ios/sync/note_sync_service_factory.h"
#import "sync/notes/note_sync_service.h"

namespace vivaldi {

NotesModelFactory::NotesModelFactory()
    : ProfileKeyedServiceFactoryIOS("Notes_Model",
                                    ProfileSelection::kRedirectedInIncognito,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(NoteSyncServiceFactory::GetInstance());
  DependsOn(SyncedFileStoreFactory::GetInstance());
}

NotesModelFactory::~NotesModelFactory() {}

// static
NotesModel* NotesModelFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<NotesModel>(
      profile, /*create=*/true);
}

// static
NotesModel* NotesModelFactory::GetForProfileIfExists(ProfileIOS* profile) {
  // Since this is called as part of destroying the browser state, we need this
  // extra test to avoid running into code that tests whether the browser state
  // is still valid.
  if (!GetInstance()->IsServiceCreated(profile)) {
    return nullptr;
  }
  return GetInstance()->GetServiceForProfileAs<NotesModel>(
      profile, /*create=*/false);
}

// static
NotesModelFactory* NotesModelFactory::GetInstance() {
  static base::NoDestructor<NotesModelFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService> NotesModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  auto notes_model = std::make_unique<NotesModel>(
      NoteSyncServiceFactory::GetForProfile(profile),
      SyncedFileStoreFactory::GetForProfile(profile));
  notes_model->Load(profile->GetStatePath());
  return notes_model;
}

void NotesModelFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {}

}  // namespace vivaldi
