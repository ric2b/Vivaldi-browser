// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/sync/vivaldi_sync_invalidations_service_factory.h"

#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/shared/model/application_context/application_context.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/vivaldi_account/vivaldi_account_manager_factory.h"
#include "prefs/vivaldi_pref_names.h"
#include "sync/invalidations/vivaldi_sync_invalidations_service.h"

namespace vivaldi {

syncer::SyncInvalidationsService*
VivaldiSyncInvalidationsServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<syncer::SyncInvalidationsService*>(
      GetInstance()->GetServiceForBrowserState(profile, /*create=*/true));
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
VivaldiSyncInvalidationsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<VivaldiSyncInvalidationsService>(
      GetApplicationContext()->GetLocalState()->GetString(
          vivaldiprefs::kVivaldiSyncNotificationsServerUrl),
      VivaldiAccountManagerFactory::GetForProfile(profile),
      base::BindRepeating([]() {
        // Probably OK to use the system network context here, since the
        // websocket connection established for invalidation doesn't rely
        // on anything locally stored.
        return GetApplicationContext()->GetSystemNetworkContext();
      }));
}
}  // namespace vivaldi
