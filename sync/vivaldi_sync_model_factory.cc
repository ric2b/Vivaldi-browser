// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_model_factory.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "sync/vivaldi_syncmanager.h"
#include "sync/vivaldi_syncmanager_factory.h"

// static
VivaldiSyncModel* SyncModelFactory::GetForProfile(Profile* profile) {
  return static_cast<VivaldiSyncModel*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
VivaldiSyncModel* SyncModelFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<VivaldiSyncModel*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
SyncModelFactory* SyncModelFactory::GetInstance() {
  return base::Singleton<SyncModelFactory>::get();
}

SyncModelFactory::SyncModelFactory()
    : BrowserContextKeyedServiceFactory(
          "SyncModel",
          BrowserContextDependencyManager::GetInstance()) {}

SyncModelFactory::~SyncModelFactory() {}

KeyedService* SyncModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  vivaldi::VivaldiSyncManager* client =
      vivaldi::VivaldiSyncManagerFactory::GetForProfileVivaldi(profile);

  VivaldiSyncModel* sync_model = new VivaldiSyncModel(client);
  client->Init(sync_model);

  return sync_model;
}

void SyncModelFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {}

content::BrowserContext* SyncModelFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool SyncModelFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
