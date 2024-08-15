// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "vivaldi_account/vivaldi_account_manager_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/storage_partition.h"
#include "vivaldi_account/vivaldi_account_manager.h"

namespace vivaldi {

// static
VivaldiAccountManager* VivaldiAccountManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<VivaldiAccountManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
VivaldiAccountManagerFactory* VivaldiAccountManagerFactory::GetInstance() {
  return base::Singleton<VivaldiAccountManagerFactory>::get();
}

VivaldiAccountManagerFactory::VivaldiAccountManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "VivaldiAccountManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfilePasswordStoreFactory::GetInstance());
}

VivaldiAccountManagerFactory::~VivaldiAccountManagerFactory() {}

KeyedService* VivaldiAccountManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto url_loader_factory = profile->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  auto password_store = ProfilePasswordStoreFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);

  return new VivaldiAccountManager(
      profile->GetPrefs(), g_browser_process->local_state(),
      std::move(url_loader_factory), std::move(password_store));
}

}  // namespace vivaldi
