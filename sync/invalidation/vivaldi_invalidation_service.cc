// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "sync/invalidation/vivaldi_invalidation_service.h"

#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/invalidation/impl/invalidation_service_util.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/public/topic_data.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "vivaldi_account/vivaldi_account_manager_factory.h"

// The sender id is only used to storeand retrieve prefs related to the
// validation handler. As long as it doesn't match any id used in chromium,
// any value is fine.
namespace {
const char kDummySenderId[] = "0000000000";

constexpr net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    5000,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.1,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    1000 * 60 * 5,  // 5 minutes.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

constexpr char kNotificationServerVHost[] = "sync";
constexpr char kNotificationChannelPrefix[] = "/exchange/notify:";
}  // namespace

namespace vivaldi {

VivaldiInvalidationService::VivaldiInvalidationService(
    PrefService* prefs,
    const std::string& notification_server_url,
    VivaldiAccountManager* account_manager,
    NetworkContextProvider network_context_provider)
    : notification_server_url_(notification_server_url),
      account_manager_(account_manager),
      network_context_provider_(network_context_provider),
      websocket_backoff_(&kBackoffPolicy),
      client_id_(invalidation::GenerateInvalidatorClientId()),
      invalidator_registrar_(prefs, kDummySenderId, false) {
  DCHECK(account_manager_);
  account_manager_->AddObserver(this);
}

VivaldiInvalidationService::~VivaldiInvalidationService() {}

bool VivaldiInvalidationService::ConnectionAllowed() {
  return account_manager_ && !account_manager_->access_token().empty() &&
         !invalidator_registrar_.GetAllSubscribedTopics().empty();
}

void VivaldiInvalidationService::ToggleConnectionIfNeeded() {
  if (websocket_backoff_timer_.IsRunning())
    return;
  DCHECK(!websocket_backoff_.ShouldRejectRequest());
  if (stomp_web_socket_ && !ConnectionAllowed())
    stomp_web_socket_.reset();
  else if (!stomp_web_socket_ && ConnectionAllowed())
    stomp_web_socket_ = std::make_unique<InvalidationServiceStompWebsocket>(
        network_context_provider_.Run(), notification_server_url_, this);
}

void VivaldiInvalidationService::RegisterInvalidationHandler(
    invalidation::InvalidationHandler* handler) {
  invalidator_registrar_.RegisterHandler(handler);
  handler->OnInvalidatorClientIdChange(client_id_);
}

bool VivaldiInvalidationService::UpdateInterestedTopics(
    invalidation::InvalidationHandler* handler,
    const invalidation::TopicSet& legacy_topic_set) {
  std::set<invalidation::TopicData> topic_set;
  for (const auto& topic_name : legacy_topic_set) {
    topic_set.insert(invalidation::TopicData(
        topic_name, handler->IsPublicTopic(topic_name)));
  }
  bool result =
      invalidator_registrar_.UpdateRegisteredTopics(handler, topic_set);
  ToggleConnectionIfNeeded();
  return result;
}

void VivaldiInvalidationService::UnsubscribeFromUnregisteredTopics(
    invalidation::InvalidationHandler* handler) {
  invalidator_registrar_.RemoveUnregisteredTopics(handler);
  ToggleConnectionIfNeeded();
}

void VivaldiInvalidationService::UnregisterInvalidationHandler(
    invalidation::InvalidationHandler* handler) {
  invalidator_registrar_.UnregisterHandler(handler);
}

invalidation::InvalidatorState VivaldiInvalidationService::GetInvalidatorState()
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
    base::RepeatingCallback<void(base::Value::Dict)> caller) const {
  base::Value::Dict value;
  std::move(caller).Run(std::move(value));
}

void VivaldiInvalidationService::PerformInvalidation(
    const invalidation::TopicInvalidationMap& invalidation_map) {
  invalidator_registrar_.DispatchInvalidationsToHandlers(invalidation_map);
}

void VivaldiInvalidationService::UpdateInvalidatorState(
    invalidation::InvalidatorState state) {
  invalidator_registrar_.UpdateInvalidatorState(state);
}

void VivaldiInvalidationService::OnVivaldiAccountUpdated() {
  ToggleConnectionIfNeeded();
}

void VivaldiInvalidationService::OnTokenFetchSucceeded() {
  ToggleConnectionIfNeeded();
}

void VivaldiInvalidationService::OnVivaldiAccountShutdown() {
  account_manager_->RemoveObserver(this);
  account_manager_ = nullptr;
  // Will Close the connection
  ToggleConnectionIfNeeded();
}

std::string VivaldiInvalidationService::GetLogin() {
  return account_manager_->access_token();
}

std::string VivaldiInvalidationService::GetVHost() {
  return kNotificationServerVHost;
}

std::string VivaldiInvalidationService::GetChannel() {
  return kNotificationChannelPrefix +
         account_manager_->account_info().account_id;
}
void VivaldiInvalidationService::OnConnected() {
  UpdateInvalidatorState(invalidation::INVALIDATIONS_ENABLED);
  websocket_backoff_.InformOfRequest(true);
}

void VivaldiInvalidationService::OnClosed() {
  UpdateInvalidatorState(invalidation::TRANSIENT_INVALIDATION_ERROR);
  websocket_backoff_.InformOfRequest(false);
  stomp_web_socket_.reset();
  // Unretained ok, because the callback is owned by the timer, which is owned
  // by this.
  websocket_backoff_timer_.Start(
      FROM_HERE, websocket_backoff_.GetTimeUntilRelease(),
      base::BindOnce(&VivaldiInvalidationService::ToggleConnectionIfNeeded,
                     base::Unretained(this)));
}

void VivaldiInvalidationService::OnInvalidation(
    base::Value::Dict invalidation) {
  constexpr char kClientIdKey[] = "client_id";
  constexpr char kVersionKey[] = "version";
  constexpr char kTypeKey[] = "notification_type";

  std::string* client_id = invalidation.FindString(kClientIdKey);
  absl::optional<int64_t> version =
      base::ValueToInt64(invalidation.Find(kVersionKey));
  std::string* type = invalidation.FindString(kTypeKey);

  if (client_id && version && type && *client_id != client_id_) {
    invalidation::TopicInvalidationMap invalidations;
    invalidations.Insert(invalidation::Invalidation::Init(
        invalidation::Topic(*type), *version, ""));
    PerformInvalidation(invalidations);
  }
}

}  // namespace vivaldi
