// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "menus/menu_service_factory.h"

#include "base/deferred_sequenced_task_runner.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "menus/menu_model.h"

namespace menus {

MenuServiceFactory::MenuServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "MenuService",
          BrowserContextDependencyManager::GetInstance()) {}

MenuServiceFactory::~MenuServiceFactory() {}

// static
Menu_Model* MenuServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<Menu_Model*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
Menu_Model* MenuServiceFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context) {
  return static_cast<Menu_Model*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

// static
MenuServiceFactory* MenuServiceFactory::GetInstance() {
  return base::Singleton<MenuServiceFactory>::get();
}

// static
void MenuServiceFactory::ShutdownForProfile(Profile* profile) {
  MenuServiceFactory* factory = GetInstance();
  factory->BrowserContextDestroyed(profile);
}

content::BrowserContext* MenuServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* MenuServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  std::unique_ptr<Menu_Model> service(new Menu_Model(context));
  service->Load(Profile::FromBrowserContext(context)->GetIOTaskRunner());
  return service.release();
}

bool MenuServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace menus
