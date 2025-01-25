// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mail_client_service_factory.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"

#include "mail_client_service.h"
#include "prefs/vivaldi_pref_names.h"

namespace mail_client {

MailClientServiceFactory::MailClientServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "MailClientService",
          BrowserContextDependencyManager::GetInstance()) {}

MailClientServiceFactory::~MailClientServiceFactory() {}

// static
MailClientService* MailClientServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<MailClientService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MailClientService* MailClientServiceFactory::GetForProfileIfExists(
    Profile* profile,
    ServiceAccessType sat) {
  return static_cast<MailClientService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
MailClientService* MailClientServiceFactory::GetForProfileWithoutCreating(
    Profile* profile) {
  return static_cast<MailClientService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
MailClientServiceFactory* MailClientServiceFactory::GetInstance() {
  return base::Singleton<MailClientServiceFactory>::get();
}

// static
void MailClientServiceFactory::ShutdownForProfile(Profile* profile) {
  MailClientServiceFactory* factory = GetInstance();
  factory->BrowserContextDestroyed(profile);
}

content::BrowserContext* MailClientServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* MailClientServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  std::unique_ptr<MailClientService> mail_client_service(
      new MailClientService());

  Profile* profile = Profile::FromBrowserContext(context);

  MailClientDatabaseParams param = MailClientDatabaseParams(profile->GetPath());

  if (!mail_client_service->Init(false, param)) {
    return nullptr;
  }
  return mail_client_service.release();
}

bool MailClientServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace mail_client
