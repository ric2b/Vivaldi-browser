// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vivaldi_status/vivaldi_status_factory.h"

#include "base/task/deferred_sequenced_task_runner.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "vivaldi_status/vivaldi_status.h"

namespace vivaldi_status {

VivaldiStatusFactory::VivaldiStatusFactory()
  : BrowserContextKeyedServiceFactory(
      "VivaldiStatus",
      BrowserContextDependencyManager::GetInstance()) {}

VivaldiStatusFactory::~VivaldiStatusFactory() {}

// static
VivaldiStatus* VivaldiStatusFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<VivaldiStatus*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
VivaldiStatus* VivaldiStatusFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context) {
  return static_cast<VivaldiStatus*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

// static
VivaldiStatusFactory* VivaldiStatusFactory::GetInstance() {
  return base::Singleton<VivaldiStatusFactory>::get();
}

// static
void VivaldiStatusFactory::ShutdownForProfile(Profile* profile) {
  VivaldiStatusFactory* factory = GetInstance();
  factory->BrowserContextDestroyed(profile);
}

content::BrowserContext* VivaldiStatusFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

std::unique_ptr<KeyedService>
VivaldiStatusFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  std::unique_ptr<VivaldiStatus> service(new VivaldiStatus());
  service->Init(context, true);
  return service;
}

bool VivaldiStatusFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace vivaldi_status
