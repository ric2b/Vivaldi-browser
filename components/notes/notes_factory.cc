// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved

#include "components/notes/notes_factory.h"

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "components/notes/note_node.h"
#include "components/notes/notes_model.h"
#include "sync/file_sync/file_store_factory.h"
#include "sync/note_sync_service_factory.h"
#include "sync/notes/note_sync_service.h"

namespace vivaldi {

NotesModelFactory::NotesModelFactory()
    : BrowserContextKeyedServiceFactory(
          "Notes_Model",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(NoteSyncServiceFactory::GetInstance());
  DependsOn(SyncedFileStoreFactory::GetInstance());
}

NotesModelFactory::~NotesModelFactory() {}

// static
NotesModel* NotesModelFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<NotesModel*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
NotesModel* NotesModelFactory::GetForBrowserContextIfExists(
    content::BrowserContext* browser_context) {
  return static_cast<NotesModel*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, false));
}

// static
NotesModelFactory* NotesModelFactory::GetInstance() {
  return base::Singleton<NotesModelFactory>::get();
}

KeyedService* NotesModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  NotesModel* notes_model =
      new NotesModel(NoteSyncServiceFactory::GetForProfile(profile),
                     SyncedFileStoreFactory::GetForBrowserContext(profile));
  notes_model->Load(profile->GetPath());
  return notes_model;
}

void NotesModelFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {}

content::BrowserContext* NotesModelFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

bool NotesModelFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace vivaldi
