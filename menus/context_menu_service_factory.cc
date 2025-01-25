// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "menus/context_menu_service_factory.h"

#include "base/task/deferred_sequenced_task_runner.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "menus/menu_model.h"

namespace menus {

ContextMenuServiceFactory::ContextMenuServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ContextMenuService",
          BrowserContextDependencyManager::GetInstance()) {}

ContextMenuServiceFactory::~ContextMenuServiceFactory() {}

// static
Menu_Model* ContextMenuServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<Menu_Model*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
Menu_Model* ContextMenuServiceFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context) {
  return static_cast<Menu_Model*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

// static
ContextMenuServiceFactory* ContextMenuServiceFactory::GetInstance() {
  return base::Singleton<ContextMenuServiceFactory>::get();
}

// static
void ContextMenuServiceFactory::ShutdownForProfile(Profile* profile) {
  GetInstance()->BrowserContextDestroyed(profile);
}

content::BrowserContext* ContextMenuServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* ContextMenuServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  std::unique_ptr<Menu_Model> service(
      new Menu_Model(context, Menu_Model::kContextMenu));
  return service.release();
}

bool ContextMenuServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace menus
