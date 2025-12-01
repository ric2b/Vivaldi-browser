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
#include "omnibox_service.h"

namespace vivaldi_omnibox {

using vivaldi_omnibox::OmniboxService;
using vivaldi_omnibox::OmniboxServiceFactory;

// static
OmniboxServiceFactory::OmniboxServiceFactory()
    : ProfileKeyedServiceFactory(
          "OmniboxServiceFactory",
          ProfileSelections::Builder()
              //`kOwnInstance`: Always returns itself, both Original and
              // OTR profiles.
              .WithRegular(ProfileSelection::kOwnInstance)
              // We need both regular and incognito because the guest profile
              // start as regular and swiches to ignognito before loading our
              // ui.
              .WithGuest(ProfileSelection::kOwnInstance)
              // No service for system profile.
              .WithSystem(ProfileSelection::kNone)
              .Build()) {}

OmniboxServiceFactory::~OmniboxServiceFactory() {}

// static
OmniboxService* OmniboxServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<OmniboxService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
OmniboxServiceFactory* OmniboxServiceFactory::GetInstance() {
  static base::NoDestructor<OmniboxServiceFactory> instance;
  return instance.get();
}

// static
void OmniboxServiceFactory::ShutdownForProfile(Profile* profile) {
  OmniboxServiceFactory* factory = GetInstance();
  factory->BrowserContextDestroyed(profile);
}


bool OmniboxServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
OmniboxServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  std::unique_ptr<OmniboxService> omnibox_service =
      std::make_unique<OmniboxService>(profile);

  return omnibox_service;
}

}  // namespace vivaldi_omnibox
