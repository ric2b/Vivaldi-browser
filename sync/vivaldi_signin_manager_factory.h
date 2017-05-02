// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIVALDI_SIGNIN_MANAGER_FACTORY_H_
#define VIVALDI_SIGNIN_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

#include "sync/vivaldi_signin_manager.h"

class SigninManagerBase;
class PrefRegistrySimple;
class Profile;

namespace vivaldi {

class VivaldiSigninManager;

// Singleton that owns all VivaldiSigninManagers and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated VivaldiSigninManager.
class VivaldiSigninManagerFactory : public SigninManagerFactory {
 public:
  static VivaldiSigninManager* GetForProfile(Profile* profile);
  static VivaldiSigninManager* GetForProfileIfExists(Profile* profile);

  // Returns an instance of the VivaldiSigninManagerFactory singleton.
  static VivaldiSigninManagerFactory* GetInstance();

  // Implementation of BrowserContextKeyedServiceFactory (public so tests
  // can call it).
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  // Registers the browser-global prefs used by VivaldiSigninManager.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  friend struct base::DefaultSingletonTraits<VivaldiSigninManagerFactory>;

  VivaldiSigninManagerFactory();
  ~VivaldiSigninManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  void BrowserContextShutdown(content::BrowserContext* context) override;
};

}  // namespace vivaldi

#endif  // VIVALDI_SIGNIN_MANAGER_FACTORY_H_
