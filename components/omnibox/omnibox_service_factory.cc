// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "omnibox_service.h"

namespace vivaldi_omnibox {

using vivaldi_omnibox::OmniboxService;
using vivaldi_omnibox::OmniboxServiceFactory;

// static
OmniboxServiceFactory::OmniboxServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "OmniboxService",
          BrowserContextDependencyManager::GetInstance()) {}

OmniboxServiceFactory::~OmniboxServiceFactory() {}

// static
OmniboxService* OmniboxServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<OmniboxService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
OmniboxServiceFactory* OmniboxServiceFactory::GetInstance() {
  return base::Singleton<OmniboxServiceFactory>::get();
}

// static
void OmniboxServiceFactory::ShutdownForProfile(Profile* profile) {
  OmniboxServiceFactory* factory = GetInstance();
  factory->BrowserContextDestroyed(profile);
}

KeyedService* OmniboxServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  OmniboxService* omnibox_service = new OmniboxService(profile);

  return omnibox_service;
}

content::BrowserContext* OmniboxServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace vivaldi_omnibox
