// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_invalidation_service.h"

#include <string>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/invalidation/impl/invalidation_service_util.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/public/topic_data.h"
#include "components/invalidation/public/topic_invalidation_map.h"

// The sender id is only used to storeand retrieve prefs related to the
// validation handler. As long as it doesn't match any id used in chromium,
// any value is fine.
namespace {
const char kSummySenderId[] = "0000000000";
}

namespace vivaldi {

VivaldiInvalidationService::VivaldiInvalidationService(Profile* profile)
    : client_id_(invalidation::GenerateInvalidatorClientId()),
      invalidator_registrar_(profile->GetPrefs(), kSummySenderId, false) {}

VivaldiInvalidationService::~VivaldiInvalidationService() {}

void VivaldiInvalidationService::RegisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  invalidator_registrar_.RegisterHandler(handler);
}

bool VivaldiInvalidationService::UpdateInterestedTopics(
    syncer::InvalidationHandler* handler,
    const syncer::TopicSet& legacy_topic_set) {
  std::set<invalidation::TopicData> topic_set;
  for (const auto& topic_name : legacy_topic_set) {
    topic_set.insert(invalidation::TopicData(
        topic_name, handler->IsPublicTopic(topic_name)));
  }
  return invalidator_registrar_.UpdateRegisteredTopics(handler, topic_set);
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

void VivaldiInvalidationService::PerformInvalidation(
    const syncer::TopicInvalidationMap& invalidation_map) {
  invalidator_registrar_.DispatchInvalidationsToHandlers(invalidation_map);
}

void VivaldiInvalidationService::UpdateInvalidatorState(
    syncer::InvalidatorState state) {
  invalidator_registrar_.UpdateInvalidatorState(state);
}

}  // namespace vivaldi
