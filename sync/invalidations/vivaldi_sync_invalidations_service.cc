// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "sync/invalidations/vivaldi_sync_invalidations_service.h"

#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/sync/invalidations/fcm_registration_token_observer.h"
#include "components/sync/invalidations/interested_data_types_handler.h"
#include "components/sync/invalidations/invalidations_listener.h"
#include "vivaldi_account/vivaldi_account_manager_factory.h"

// The sender id is only used to store and retrieve prefs related to the
// validation handler. As long as it doesn't match any id used in chromium,
// any value is fine.
namespace {
// Limits the number of last received buffered messages.
const size_t kMaxBufferedLastFcmMessages = 20;

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
    0.1,  // 10%

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

VivaldiSyncInvalidationsService::VivaldiSyncInvalidationsService(
    const std::string& notification_server_url,
    VivaldiAccountManager* account_manager,
    NetworkContextProvider network_context_provider)
    : notification_server_url_(notification_server_url),
      account_manager_(account_manager),
      network_context_provider_(network_context_provider),
      stomp_client_backoff_(&kBackoffPolicy) {
  DCHECK(account_manager_);
  account_manager_->AddObserver(this);
}

VivaldiSyncInvalidationsService::~VivaldiSyncInvalidationsService() {}

void VivaldiSyncInvalidationsService::ToggleConnectionIfNeeded() {
  bool connection_allowed = account_manager_ &&
                            !account_manager_->access_token().empty() &&
                            invalidations_requested_;
  if (stomp_client_backoff_timer_.IsRunning())
    return;
  DCHECK(!stomp_client_backoff_.ShouldRejectRequest());
  if (stomp_client_ && !connection_allowed)
    stomp_client_.reset();
  else if (!stomp_client_ && connection_allowed)
    stomp_client_ = std::make_unique<InvalidationServiceStompClient>(
        network_context_provider_.Run(), notification_server_url_, this);
}

void VivaldiSyncInvalidationsService::StartListening() {
  invalidations_requested_ = true;
  ToggleConnectionIfNeeded();
}

void VivaldiSyncInvalidationsService::StopListening() {
  StopListeningPermanently();
}

void VivaldiSyncInvalidationsService::StopListeningPermanently() {
  invalidations_requested_ = false;
  ToggleConnectionIfNeeded();
}

void VivaldiSyncInvalidationsService::AddListener(
    syncer::InvalidationsListener* listener) {
  if (listeners_.HasObserver(listener)) {
    return;
  }
  listeners_.AddObserver(listener);

  // Immediately replay any buffered messages received before the |listener|
  // was added.
  for (const std::string& message : last_received_messages_) {
    listener->OnInvalidationReceived(message);
  }
}

bool VivaldiSyncInvalidationsService::HasListener(
    syncer::InvalidationsListener* listener) {
  return listeners_.HasObserver(listener);
}

void VivaldiSyncInvalidationsService::RemoveListener(
    syncer::InvalidationsListener* listener) {
  listeners_.RemoveObserver(listener);
}

void VivaldiSyncInvalidationsService::AddTokenObserver(
    syncer::FCMRegistrationTokenObserver* observer) {
  token_observers_.AddObserver(observer);
}

void VivaldiSyncInvalidationsService::RemoveTokenObserver(
    syncer::FCMRegistrationTokenObserver* observer) {
  token_observers_.RemoveObserver(observer);
}

std::optional<std::string>
VivaldiSyncInvalidationsService::GetFCMRegistrationToken() const {
  if (!stomp_client_) {
    return std::nullopt;
  }
  std::string session_id = stomp_client_->session_id();
  if (session_id.empty()) {
    return std::nullopt;
  }

  return session_id;
}

void VivaldiSyncInvalidationsService::OnVivaldiAccountUpdated() {
  ToggleConnectionIfNeeded();
}

void VivaldiSyncInvalidationsService::OnTokenFetchSucceeded() {
  ToggleConnectionIfNeeded();
}

void VivaldiSyncInvalidationsService::OnVivaldiAccountShutdown() {
  account_manager_->RemoveObserver(this);
  account_manager_ = nullptr;
  // Will Close the connection
  ToggleConnectionIfNeeded();
}

std::string VivaldiSyncInvalidationsService::GetLogin() const {
  return account_manager_->access_token();
}

std::string VivaldiSyncInvalidationsService::GetVHost() const {
  return kNotificationServerVHost;
}

std::string VivaldiSyncInvalidationsService::GetChannel() const {
  return kNotificationChannelPrefix +
         account_manager_->account_info().account_id;
}
void VivaldiSyncInvalidationsService::OnConnected() {
  stomp_client_backoff_.InformOfRequest(true);

  for (syncer::FCMRegistrationTokenObserver& token_observer :
       token_observers_) {
    token_observer.OnFCMRegistrationTokenChanged();
  }
}

void VivaldiSyncInvalidationsService::OnClosed() {
  stomp_client_backoff_.InformOfRequest(false);

  stomp_client_.reset();

  for (syncer::FCMRegistrationTokenObserver& token_observer :
       token_observers_) {
    token_observer.OnFCMRegistrationTokenChanged();
  }

  // Unretained ok, because the callback is owned by the timer, which is owned
  // by this.
  stomp_client_backoff_timer_.Start(
      FROM_HERE, stomp_client_backoff_.GetTimeUntilRelease(),
      base::BindOnce(&VivaldiSyncInvalidationsService::ToggleConnectionIfNeeded,
                     base::Unretained(this)));
}

void VivaldiSyncInvalidationsService::OnInvalidation(std::string message) {
  if (last_received_messages_.size() >= kMaxBufferedLastFcmMessages) {
    last_received_messages_.pop_front();
  }
  last_received_messages_.push_back(message);
  for (syncer::InvalidationsListener& listener : listeners_) {
    listener.OnInvalidationReceived(message);
  }
}

void VivaldiSyncInvalidationsService::SetInterestedDataTypesHandler(
    syncer::InterestedDataTypesHandler* handler) {
  DCHECK(!interested_data_types_handler_ || !handler);
  interested_data_types_handler_ = handler;
}

std::optional<syncer::DataTypeSet>
VivaldiSyncInvalidationsService::GetInterestedDataTypes() const {
  return interested_data_types_;
}

void VivaldiSyncInvalidationsService::SetInterestedDataTypes(
    const syncer::DataTypeSet& data_types) {
  DCHECK(interested_data_types_handler_);

  interested_data_types_ = data_types;
  interested_data_types_handler_->OnInterestedDataTypesChanged();
}

void VivaldiSyncInvalidationsService::
    SetCommittedAdditionalInterestedDataTypesCallback(
        InterestedDataTypesAppliedCallback callback) {
  DCHECK(interested_data_types_handler_);

  interested_data_types_handler_
      ->SetCommittedAdditionalInterestedDataTypesCallback(std::move(callback));
}

}  // namespace vivaldi
