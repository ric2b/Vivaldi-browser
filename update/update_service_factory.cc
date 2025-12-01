// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_service_factory.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"

#include "prefs/vivaldi_pref_names.h"
#include "update/update_service.h"

namespace update {

UpdateServiceFactory::UpdateServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "UpdateService",
          BrowserContextDependencyManager::GetInstance()) {}

UpdateServiceFactory::~UpdateServiceFactory() {}

// static
UpdateService* UpdateServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<UpdateService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
UpdateService* UpdateServiceFactory::GetForProfileIfExists(
    Profile* profile,
    ServiceAccessType sat) {
  return static_cast<UpdateService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
UpdateService* UpdateServiceFactory::GetForProfileWithoutCreating(
    Profile* profile) {
  return static_cast<UpdateService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
UpdateServiceFactory* UpdateServiceFactory::GetInstance() {
  return base::Singleton<UpdateServiceFactory>::get();
}

// static
void UpdateServiceFactory::ShutdownForProfile(Profile* profile) {
  UpdateServiceFactory* factory = GetInstance();
  factory->BrowserContextDestroyed(profile);
}

content::BrowserContext* UpdateServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

std::unique_ptr<KeyedService>
UpdateServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  std::unique_ptr<update::UpdateService> update_service(
      new update::UpdateService());

  if (!update_service->Init()) {
    return nullptr;
  }
  return update_service;
}

bool UpdateServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace update
