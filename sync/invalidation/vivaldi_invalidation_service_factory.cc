// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/invalidation/vivaldi_invalidation_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/storage_partition.h"
#include "prefs/vivaldi_pref_names.h"
#include "sync/invalidation/vivaldi_invalidation_service.h"
#include "vivaldi_account/vivaldi_account_manager_factory.h"

namespace vivaldi {

invalidation::InvalidationService*
VivaldiInvalidationServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<VivaldiInvalidationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

VivaldiInvalidationServiceFactory*
VivaldiInvalidationServiceFactory::GetInstance() {
  static base::NoDestructor<VivaldiInvalidationServiceFactory> instance;
  return instance.get();
}

VivaldiInvalidationServiceFactory::VivaldiInvalidationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "VivaldiInvalidationsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(VivaldiAccountManagerFactory::GetInstance());
}

VivaldiInvalidationServiceFactory::~VivaldiInvalidationServiceFactory() =
    default;

KeyedService* VivaldiInvalidationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new VivaldiInvalidationService(
      profile->GetPrefs(),
      g_browser_process->local_state()->GetString(
          vivaldiprefs::kVivaldiSyncNotificationsServerUrl),
      VivaldiAccountManagerFactory::GetForProfile(profile),
      base::BindRepeating(
          [](content::BrowserContext* context) {
            return context->GetDefaultStoragePartition()->GetNetworkContext();
          },
          // Unretained OK, because the service can't outlive the browser
          // context.
          base::Unretained(context)));
}
}  // namespace vivaldi
