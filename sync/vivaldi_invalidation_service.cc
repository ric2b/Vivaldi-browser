// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_invalidation_service.h"

#include <string>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/invalidation/impl/invalidation_service_util.h"
#include "components/signin/core/browser/profile_identity_provider.h"
#include "sync/vivaldi_profile_oauth2_token_service_factory.h"
#include "sync/vivaldi_signin_manager_factory.h"

namespace vivaldi {

VivaldiInvalidationService::VivaldiInvalidationService(Profile* profile)
    : client_id_(invalidation::GenerateInvalidatorClientId()),
      identity_provider_(new ProfileIdentityProvider(
          VivaldiSigninManagerFactory::GetForProfile(profile),
          VivaldiProfileOAuth2TokenServiceFactory::GetForProfile(profile),
          base::Closure())) {}

VivaldiInvalidationService::~VivaldiInvalidationService() {}

void VivaldiInvalidationService::RegisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  invalidator_registrar_.RegisterHandler(handler);
}

bool VivaldiInvalidationService::UpdateRegisteredInvalidationIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  return invalidator_registrar_.UpdateRegisteredIds(handler, ids);
}

void VivaldiInvalidationService::UnregisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  invalidator_registrar_.UnregisterHandler(handler);
}

syncer::InvalidatorState VivaldiInvalidationService::GetInvalidatorState()
    const {
  return invalidator_registrar_.GetInvalidatorState();
}

std::string VivaldiInvalidationService::GetInvalidatorClientId() const {
  return client_id_;
}

invalidation::InvalidationLogger*
VivaldiInvalidationService::GetInvalidationLogger() {
  return nullptr;
}

void VivaldiInvalidationService::RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> caller) const {
  base::DictionaryValue value;
  caller.Run(value);
}

IdentityProvider* VivaldiInvalidationService::GetIdentityProvider() {
  return identity_provider_.get();
}

void VivaldiInvalidationService::PerformInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  invalidator_registrar_.DispatchInvalidationsToHandlers(invalidation_map);
}

}  // namespace vivaldi
