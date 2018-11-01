// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved

#include "base/deferred_sequenced_task_runner.h"
#include "base/memory/singleton.h"
#include "chrome/browser/bookmarks/startup_task_runner_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/startup_task_runner_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"

#include "notes/notes_factory.h"
#include "notes/notes_model.h"
#include "notes/notesnode.h"

namespace vivaldi {

NotesModelFactory::NotesModelFactory()
    : BrowserContextKeyedServiceFactory(
          "Notes_Model",
          BrowserContextDependencyManager::GetInstance()) {}

NotesModelFactory::~NotesModelFactory() {}

// static
Notes_Model* NotesModelFactory::GetForProfile(Profile* profile) {
  return static_cast<Notes_Model*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
Notes_Model* NotesModelFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<Notes_Model*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
NotesModelFactory* NotesModelFactory::GetInstance() {
  return base::Singleton<NotesModelFactory>::get();
}

KeyedService* NotesModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  Notes_Model* notes_model = new Notes_Model(profile);
  notes_model->Load(profile->GetIOTaskRunner());
  return notes_model;
}

void NotesModelFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {}

content::BrowserContext* NotesModelFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool NotesModelFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace vivaldi
