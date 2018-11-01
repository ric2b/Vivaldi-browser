// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_VIVALDI_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define SYNC_VIVALDI_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"

#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"

namespace vivaldi {

class VivaldiSyncManager;

class VivaldiProfileOAuth2TokenService : public ProfileOAuth2TokenService {
 public:
  struct PendingRequest {
    PendingRequest();
    ~PendingRequest();

    std::string account_id;
    std::string client_id;
    std::string client_secret;
    ScopeSet scopes;
    base::WeakPtr<RequestImpl> request;
  };

  explicit VivaldiProfileOAuth2TokenService(
      std::unique_ptr<OAuth2TokenServiceDelegate> delegate);
  ~VivaldiProfileOAuth2TokenService() override;

  void SetConsumer(VivaldiSyncManager* consumer);

  // Overridden to make sure it works on iOS.
  void LoadCredentials(const std::string& primary_account_id) override;

 protected:
  // OAuth2TokenService overrides.
  void FetchOAuth2Token(RequestImpl* request,
                        const std::string& account_id,
                        net::URLRequestContextGetter* getter,
                        const std::string& client_id,
                        const std::string& client_secret,
                        const ScopeSet& scopes) override;

  void InvalidateAccessTokenImpl(const std::string& account_id,
                                 const std::string& client_id,
                                 const ScopeSet& scopes,
                                 const std::string& access_token) override;

  net::URLRequestContextGetter* GetRequestContext() const override;

 private:
  VivaldiSyncManager* consumer_;

  base::WeakPtrFactory<VivaldiProfileOAuth2TokenService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiProfileOAuth2TokenService);
};
}  // namespace vivaldi

#endif  // SYNC_VIVALDI_PROFILE_OAUTH2_TOKEN_SERVICE_H_
