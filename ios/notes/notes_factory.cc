// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#include "ios/notes/notes_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/notes/note_node.h"
#include "components/notes/notes_model.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/sync/file_store_factory.h"
#include "ios/sync/note_sync_service_factory.h"
#include "sync/notes/note_sync_service.h"

namespace vivaldi {

NotesModelFactory::NotesModelFactory()
    : BrowserStateKeyedServiceFactory(
          "Notes_Model",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(NoteSyncServiceFactory::GetInstance());
  DependsOn(SyncedFileStoreFactory::GetInstance());
}

NotesModelFactory::~NotesModelFactory() {}

// static
NotesModel* NotesModelFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<NotesModel*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
NotesModel* NotesModelFactory::GetForProfileIfExists(ProfileIOS* profile) {
  return static_cast<NotesModel*>(
      GetInstance()->GetServiceForBrowserState(profile, false));
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

web::BrowserState* NotesModelFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool NotesModelFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace vivaldi
