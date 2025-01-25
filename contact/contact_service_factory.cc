// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "contact/contact_service_factory.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"

#include "contact_service.h"
#include "prefs/vivaldi_pref_names.h"

namespace contact {

ContactServiceFactory::ContactServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ContactService",
          BrowserContextDependencyManager::GetInstance()) {}

ContactServiceFactory::~ContactServiceFactory() {}

// static
ContactService* ContactServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ContactService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ContactService* ContactServiceFactory::GetForProfileIfExists(
    Profile* profile,
    ServiceAccessType sat) {
  return static_cast<ContactService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
ContactService* ContactServiceFactory::GetForProfileWithoutCreating(
    Profile* profile) {
  return static_cast<ContactService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
ContactServiceFactory* ContactServiceFactory::GetInstance() {
  return base::Singleton<ContactServiceFactory>::get();
}

// static
void ContactServiceFactory::ShutdownForProfile(Profile* profile) {
  ContactServiceFactory* factory = GetInstance();
  factory->BrowserContextDestroyed(profile);
}

content::BrowserContext* ContactServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* ContactServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  std::unique_ptr<contact::ContactService> contact_service(
      new contact::ContactService());

  Profile* profile = Profile::FromBrowserContext(context);

  contact::ContactDatabaseParams param =
      contact::ContactDatabaseParams(profile->GetPath());

  if (!contact_service->Init(false, param)) {
    return nullptr;
  }
  return contact_service.release();
}

bool ContactServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace contact
