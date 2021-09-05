// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MENUS_MAIN_MENU_SERVICE_FACTORY_H_
#define MENUS_MAIN_MENU_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

template <typename T>
struct DefaultSingletonTraits;

namespace menus {

class Menu_Model;

// Singleton that owns all MenuModel entries and associates them with Profiles.
class MainMenuServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static Menu_Model* GetForBrowserContext(content::BrowserContext* context);

  static Menu_Model* GetForBrowserContextIfExists(
      content::BrowserContext* context);

  static MainMenuServiceFactory* GetInstance();

  static void ShutdownForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<MainMenuServiceFactory>;

  MainMenuServiceFactory();
  ~MainMenuServiceFactory() override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  bool ServiceIsNULLWhileTesting() const override;
};

}  //  namespace menus

#endif  // MENUS_MAIN_MENU_SERVICE_FACTORY_H_
