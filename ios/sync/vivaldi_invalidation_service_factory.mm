// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/sync/vivaldi_invalidation_service_factory.h"

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/shared/model/application_context/application_context.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/vivaldi_account/vivaldi_account_manager_factory.h"
#include "prefs/vivaldi_pref_names.h"
#include "sync/invalidation/vivaldi_invalidation_service.h"

namespace vivaldi {

invalidation::InvalidationService*
VivaldiInvalidationServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<VivaldiInvalidationService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

VivaldiInvalidationServiceFactory*
VivaldiInvalidationServiceFactory::GetInstance() {
  static base::NoDestructor<VivaldiInvalidationServiceFactory> instance;
  return instance.get();
}

VivaldiInvalidationServiceFactory::VivaldiInvalidationServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "VivaldiInvalidationsService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(VivaldiAccountManagerFactory::GetInstance());
}

VivaldiInvalidationServiceFactory::~VivaldiInvalidationServiceFactory() =
    default;

std::unique_ptr<KeyedService>
VivaldiInvalidationServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<VivaldiInvalidationService>(
      browser_state->GetPrefs(),
      GetApplicationContext()->GetLocalState()->GetString(
          vivaldiprefs::kVivaldiSyncNotificationsServerUrl),
      VivaldiAccountManagerFactory::GetForBrowserState(browser_state),
      base::BindRepeating([]() {
        // Probably OK to use the system network context here, since the
        // websocket connection established for invalidation doesn't rely
        // on anything locally stored.
        return GetApplicationContext()->GetSystemNetworkContext();
      }));
}
}  // namespace vivaldi
