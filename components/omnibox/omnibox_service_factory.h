// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_OMNIBOX_SERVICE_FACTORY_H_
#define COMPONENTS_OMNIBOX_OMNIBOX_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace vivaldi_omnibox {

class OmniboxService;

// Singleton that owns all OmniboxServices and associates them with
// Profiles.
class OmniboxServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static OmniboxService* GetForProfile(Profile* profile);

  static OmniboxServiceFactory* GetInstance();

  static void ShutdownForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<OmniboxServiceFactory>;

  OmniboxServiceFactory();
  ~OmniboxServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace vivaldi_omnibox

#endif  // COMPONENTS_OMNIBOX_OMNIBOX_SERVICE_FACTORY_H_
