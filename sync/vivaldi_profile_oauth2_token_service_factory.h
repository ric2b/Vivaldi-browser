// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIVALDI_SYNC_VIVALDI_PROFILE_OAUTH2_TOKEN_SERVICE_FACTORY_H_
#define VIVALDI_SYNC_VIVALDI_PROFILE_OAUTH2_TOKEN_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

#include "sync/vivaldi_profile_oauth2_token_service.h"

class ProfileOAuth2TokenService;
class Profile;

namespace vivaldi {

class VivaldiProfileOAuth2TokenService;

// Singleton that owns all ProfileOAuth2TokenServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated ProfileOAuth2TokenService.
class VivaldiProfileOAuth2TokenServiceFactory
    : public ProfileOAuth2TokenServiceFactory {
 public:
  // Returns the instance of ProfileOAuth2TokenService associated with this
  // profile (creating one if none exists). Returns NULL if this profile
  // cannot have a ProfileOAuth2TokenService (for example, if |profile| is
  // incognito).
  static VivaldiProfileOAuth2TokenService* GetForProfile(Profile* profile);

  // Returns an instance of the VivaldiProfileOAuth2TokenServiceFactory
  // singleton.
  static VivaldiProfileOAuth2TokenServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      VivaldiProfileOAuth2TokenServiceFactory>;

  VivaldiProfileOAuth2TokenServiceFactory();
  ~VivaldiProfileOAuth2TokenServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(VivaldiProfileOAuth2TokenServiceFactory);
};
}

#endif  // VIVALDI_SYNC_VIVALDI_PROFILE_OAUTH2_TOKEN_SERVICE_FACTORY_H_
