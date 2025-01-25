// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/invalidations/vivaldi_sync_invalidations_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "prefs/vivaldi_pref_names.h"
#include "sync/invalidations/vivaldi_sync_invalidations_service.h"
#include "vivaldi_account/vivaldi_account_manager_factory.h"

namespace vivaldi {

syncer::SyncInvalidationsService*
VivaldiSyncInvalidationsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<syncer::SyncInvalidationsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

VivaldiSyncInvalidationsServiceFactory*
VivaldiSyncInvalidationsServiceFactory::GetInstance() {
  static base::NoDestructor<VivaldiSyncInvalidationsServiceFactory> instance;
  return instance.get();
}

VivaldiSyncInvalidationsServiceFactory::
    VivaldiSyncInvalidationsServiceFactory() {
  DependsOn(VivaldiAccountManagerFactory::GetInstance());
}

VivaldiSyncInvalidationsServiceFactory::
    ~VivaldiSyncInvalidationsServiceFactory() = default;

std::unique_ptr<KeyedService>
VivaldiSyncInvalidationsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  return std::make_unique<VivaldiSyncInvalidationsService>(
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
