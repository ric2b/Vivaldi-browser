// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sessions/index_service_factory.h"

#include "base/task/deferred_sequenced_task_runner.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "sessions/index_model.h"

namespace sessions {

IndexServiceFactory::IndexServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "IndexService",
        BrowserContextDependencyManager::GetInstance()) {}

IndexServiceFactory::~IndexServiceFactory() {}

// static
Index_Model* IndexServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<Index_Model*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
Index_Model* IndexServiceFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context) {
  return static_cast<Index_Model*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

// static
IndexServiceFactory* IndexServiceFactory::GetInstance() {
  return base::Singleton<IndexServiceFactory>::get();
}

// static
void IndexServiceFactory::ShutdownForProfile(Profile* profile) {
  IndexServiceFactory* factory = GetInstance();
  factory->BrowserContextDestroyed(profile);
}

content::BrowserContext* IndexServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* IndexServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  std::unique_ptr<Index_Model> service(
      new Index_Model(context));
  // We do not load the model data here. That happens from the api when the
  // first request arrives.
  return service.release();
}

bool IndexServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace sessions