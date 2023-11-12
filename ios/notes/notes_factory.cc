// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#include "ios/notes/notes_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/sync/file_store_factory.h"
#include "ios/sync/note_sync_service_factory.h"
#include "notes/note_node.h"
#include "notes/notes_model.h"
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
NotesModel* NotesModelFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<NotesModel*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
NotesModel* NotesModelFactory::GetForBrowserStateIfExists(
    ChromeBrowserState* browser_state) {
  return static_cast<NotesModel*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
NotesModelFactory* NotesModelFactory::GetInstance() {
  static base::NoDestructor<NotesModelFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService> NotesModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  auto notes_model = std::make_unique<NotesModel>(
      NoteSyncServiceFactory::GetForBrowserState(browser_state),
      SyncedFileStoreFactory::GetForBrowserState(browser_state));
  notes_model->Load(browser_state->GetStatePath());
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
