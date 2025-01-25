// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "menus/main_menu_service_factory.h"

#include "base/task/deferred_sequenced_task_runner.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "menus/menu_model.h"

namespace menus {

MainMenuServiceFactory::MainMenuServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "MenuService",
          BrowserContextDependencyManager::GetInstance()) {}

MainMenuServiceFactory::~MainMenuServiceFactory() {}

// static
Menu_Model* MainMenuServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<Menu_Model*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
Menu_Model* MainMenuServiceFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context) {
  return static_cast<Menu_Model*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

// static
MainMenuServiceFactory* MainMenuServiceFactory::GetInstance() {
  return base::Singleton<MainMenuServiceFactory>::get();
}

// static
void MainMenuServiceFactory::ShutdownForProfile(Profile* profile) {
  MainMenuServiceFactory* factory = GetInstance();
  factory->BrowserContextDestroyed(profile);
}

content::BrowserContext* MainMenuServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* MainMenuServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  std::unique_ptr<Menu_Model> service(
      new Menu_Model(context, Menu_Model::kMainMenu));
  service->Load(false);
  return service.release();
}

bool MainMenuServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace menus
