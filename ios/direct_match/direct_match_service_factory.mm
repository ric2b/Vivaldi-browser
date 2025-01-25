// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/direct_match/direct_match_service_factory.h"

#import "components/direct_match/direct_match_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace direct_match {

// static
DirectMatchService* DirectMatchServiceFactory::GetForProfile(
  ProfileIOS* profile) {
  return static_cast<DirectMatchService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
DirectMatchService* DirectMatchServiceFactory::GetForProfileIfExists(
   ProfileIOS* profile) {
  // Since this is called as part of destroying the browser state, we need this
  // extra test to avoid running into code that tests whether the browser state
  // is still valid.
  if (!GetInstance()->IsServiceCreated(profile)) {
    return nullptr;
  }
  return static_cast<DirectMatchService*>(
      GetInstance()->GetServiceForBrowserState(profile, false));
}

// static
DirectMatchServiceFactory* DirectMatchServiceFactory::GetInstance() {
  static base::NoDestructor<DirectMatchServiceFactory> instance;
  return instance.get();
}

DirectMatchServiceFactory::DirectMatchServiceFactory()
  : BrowserStateKeyedServiceFactory(
        "DirectMatchService", BrowserStateDependencyManager::GetInstance()) {}

DirectMatchServiceFactory::~DirectMatchServiceFactory() {}

web::BrowserState* DirectMatchServiceFactory::GetBrowserStateToUse(
    web::BrowserState* browser_state) const {
  return GetBrowserStateRedirectedInIncognito(browser_state);
}

std::unique_ptr<KeyedService> DirectMatchServiceFactory::BuildServiceInstanceFor(
  web::BrowserState* browser_state) const {
  auto direct_match_service = std::make_unique<DirectMatchService>();
  auto URLLoaderFactory = GetApplicationContext()->GetSharedURLLoaderFactory();
  direct_match_service->Load(URLLoaderFactory);
  return direct_match_service;
}

bool DirectMatchServiceFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

}  // namespace direct_match
