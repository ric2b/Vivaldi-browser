// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#include "ios/vivaldi_account/vivaldi_account_manager_factory.h"

#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/shared/model/application_context/application_context.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "vivaldi_account/vivaldi_account_manager.h"

namespace vivaldi {

// static
VivaldiAccountManager* VivaldiAccountManagerFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<VivaldiAccountManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
VivaldiAccountManagerFactory* VivaldiAccountManagerFactory::GetInstance() {
  static base::NoDestructor<VivaldiAccountManagerFactory> instance;
  return instance.get();
}

VivaldiAccountManagerFactory::VivaldiAccountManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "VivaldiAccountManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IOSChromePasswordStoreFactory::GetInstance());
}

VivaldiAccountManagerFactory::~VivaldiAccountManagerFactory() {}

std::unique_ptr<KeyedService>
VivaldiAccountManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  auto url_loader_factory = browser_state->GetSharedURLLoaderFactory();
  auto password_store = IOSChromePasswordStoreFactory::GetForBrowserState(
      browser_state, ServiceAccessType::IMPLICIT_ACCESS);

  return std::make_unique<VivaldiAccountManager>(
      browser_state->GetPrefs(), GetApplicationContext()->GetLocalState(),
      std::move(url_loader_factory), std::move(password_store));
}

}  // namespace vivaldi
