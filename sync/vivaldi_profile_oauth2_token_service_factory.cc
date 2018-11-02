// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/vivaldi_profile_oauth2_token_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/mutable_profile_oauth2_token_service_delegate.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

#include "sync/vivaldi_profile_oauth2_token_service.h"
#include "sync/vivaldi_signin_client_factory.h"

namespace vivaldi {

VivaldiProfileOAuth2TokenServiceFactory::
    VivaldiProfileOAuth2TokenServiceFactory()
    : ProfileOAuth2TokenServiceFactory() {
#if !defined(OS_ANDROID)
  DependsOn(GlobalErrorServiceFactory::GetInstance());
#endif
  // DependsOn(WebDataServiceFactory::GetInstance());
  DependsOn(VivaldiSigninClientFactory::GetInstance());
  DependsOn(SigninErrorControllerFactory::GetInstance());
}

VivaldiProfileOAuth2TokenServiceFactory::
    ~VivaldiProfileOAuth2TokenServiceFactory() {}

VivaldiProfileOAuth2TokenService*
VivaldiProfileOAuth2TokenServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<VivaldiProfileOAuth2TokenService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
VivaldiProfileOAuth2TokenServiceFactory*
VivaldiProfileOAuth2TokenServiceFactory::GetInstance() {
  return base::Singleton<VivaldiProfileOAuth2TokenServiceFactory>::get();
}

KeyedService* VivaldiProfileOAuth2TokenServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  auto delegate = base::MakeUnique<MutableProfileOAuth2TokenServiceDelegate>(
      VivaldiSigninClientFactory::GetInstance()->GetForProfile(profile),
      SigninErrorControllerFactory::GetInstance()->GetForProfile(profile),
      AccountTrackerServiceFactory::GetInstance()->GetForProfile(profile));
  VivaldiProfileOAuth2TokenService* service =
      new VivaldiProfileOAuth2TokenService(std::move(delegate));
  return service;
}

}  // namespace vivaldi
