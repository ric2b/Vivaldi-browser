// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/vivaldi_signin_client_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#include "sync/vivaldi_signin_client.h"

namespace vivaldi {

VivaldiSigninClientFactory::VivaldiSigninClientFactory()
    : BrowserContextKeyedServiceFactory(
          "VivaldiSigninClient",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SigninErrorControllerFactory::GetInstance());
}

VivaldiSigninClientFactory::~VivaldiSigninClientFactory() {}

// static
SigninClient* VivaldiSigninClientFactory::GetForProfile(Profile* profile) {
  return static_cast<SigninClient*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
VivaldiSigninClientFactory* VivaldiSigninClientFactory::GetInstance() {
  return base::Singleton<VivaldiSigninClientFactory>::get();
}

KeyedService* VivaldiSigninClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  VivaldiSigninClient* client = new VivaldiSigninClient(
      profile, SigninErrorControllerFactory::GetForProfile(profile));
  return client;
}
}  // namespace vivaldi
