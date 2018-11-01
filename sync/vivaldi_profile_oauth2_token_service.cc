// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/vivaldi_profile_oauth2_token_service.h"

#include "app/vivaldi_apptools.h"
#include "base/message_loop/message_loop.h"
#include "sync/vivaldi_syncmanager.h"

namespace vivaldi {

VivaldiProfileOAuth2TokenService::PendingRequest::PendingRequest() {}

VivaldiProfileOAuth2TokenService::PendingRequest::~PendingRequest() {}

VivaldiProfileOAuth2TokenService::VivaldiProfileOAuth2TokenService(
    std::unique_ptr<OAuth2TokenServiceDelegate> delegate)
    : ProfileOAuth2TokenService(std::move(delegate)),
      consumer_(NULL),
      weak_ptr_factory_(this) {}

VivaldiProfileOAuth2TokenService::~VivaldiProfileOAuth2TokenService() {}

void VivaldiProfileOAuth2TokenService::SetConsumer(
    VivaldiSyncManager* consumer) {
  DCHECK(consumer);
  consumer_ = consumer;
}

void VivaldiProfileOAuth2TokenService::LoadCredentials(
    const std::string& primary_account_id) {
  // Empty implementation as VivaldiProfileOAuth2TokenService does not have any
  // credentials to load.
}

net::URLRequestContextGetter*
VivaldiProfileOAuth2TokenService::GetRequestContext() const {
  return NULL;
}

void VivaldiProfileOAuth2TokenService::FetchOAuth2Token(
    RequestImpl* request,
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes) {
  if (vivaldi::ForcedVivaldiRunning())
    ProfileOAuth2TokenService::FetchOAuth2Token(
        request, account_id, getter, client_id, client_secret, scopes);
  else
    consumer_->VivaldiTokenSuccess();
}

void VivaldiProfileOAuth2TokenService::InvalidateAccessTokenImpl(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  // Do nothing, as we don't have a cache from which to remove the token.
}
}  // namespace vivaldi
