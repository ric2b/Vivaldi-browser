// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "vivaldi_account/vivaldi_account_manager_factory.h"

#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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
  DependsOn(PasswordStoreFactory::GetInstance());
}

VivaldiAccountManagerFactory::~VivaldiAccountManagerFactory() {}

KeyedService* VivaldiAccountManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new VivaldiAccountManager(Profile::FromBrowserContext(context));
}

}  // namespace vivaldi
