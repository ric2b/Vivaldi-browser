// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIVALDI_SYNC_VIVALDI_SIGNIN_CLIENT_FACTORY_H_
#define VIVALDI_SYNC_VIVALDI_SIGNIN_CLIENT_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace vivaldi {

// Singleton that owns all ChromeSigninClients and associates them with
// Profiles.
class VivaldiSigninClientFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of SigninClient associated with this profile
  // (creating one if none exists). Returns NULL if this profile cannot have an
  // SigninClient (for example, if |profile| is incognito).
  static SigninClient* GetForProfile(Profile* profile);

  // Returns an instance of the factory singleton.
  static VivaldiSigninClientFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<VivaldiSigninClientFactory>;

  VivaldiSigninClientFactory();
  ~VivaldiSigninClientFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};
}

#endif  // VIVALDI_SYNC_VIVALDI_SIGNIN_CLIENT_FACTORY_H_
