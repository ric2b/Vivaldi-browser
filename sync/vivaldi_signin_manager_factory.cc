// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/vivaldi_signin_manager_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/local_auth.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/core/common/signin_pref_names.h"

#include "sync/vivaldi_signin_client_factory.h"
#include "sync/vivaldi_signin_manager.h"

namespace vivaldi {

VivaldiSigninManagerFactory::VivaldiSigninManagerFactory()
    : SigninManagerFactory() {
  DependsOn(VivaldiSigninClientFactory::GetInstance());
  // DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
}

VivaldiSigninManagerFactory::~VivaldiSigninManagerFactory() {}

// static
VivaldiSigninManager* VivaldiSigninManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<VivaldiSigninManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
VivaldiSigninManager* VivaldiSigninManagerFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<VivaldiSigninManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
VivaldiSigninManagerFactory* VivaldiSigninManagerFactory::GetInstance() {
  return base::Singleton<VivaldiSigninManagerFactory>::get();
}

void VivaldiSigninManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  VivaldiSigninManager::RegisterProfilePrefs(registry);
  LocalAuth::RegisterLocalAuthPrefs(registry);
  // Don't call SigninManagerFactory::RegisterProfilePrefs; duplicated calls
}

// static
void VivaldiSigninManagerFactory::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGoogleServicesUsernamePattern,
                               std::string());
}

KeyedService* VivaldiSigninManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  SigninManagerBase* service = NULL;
  Profile* profile = static_cast<Profile*>(context);
  SigninClient* client =
      VivaldiSigninClientFactory::GetInstance()->GetForProfile(profile);
  service = new VivaldiSigninManager(
      client, AccountTrackerServiceFactory::GetForProfile(profile));
  service->Initialize(g_browser_process->local_state());
  for (auto& observer : observer_list_)
    observer.SigninManagerCreated(service);
  return service;
}

void VivaldiSigninManagerFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  SigninManagerBase* manager = static_cast<SigninManagerBase*>(
      GetServiceForBrowserContext(context, false));
  if (manager)
    for (auto& observer : observer_list_)
      observer.SigninManagerShutdown(manager);
  BrowserContextKeyedServiceFactory::BrowserContextShutdown(context);
}
}
