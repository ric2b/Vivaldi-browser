// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The signin manager encapsulates some functionality tracking
// which user is signed in. See SigninManagerBase for full description of
// responsibilities. The class defined in this file provides functionality
// required by all platforms except Chrome OS.
//
// When a user is signed in, a ClientLogin request is run on their behalf.
// Auth tokens are fetched from Google and the results are stored in the
// TokenService.
// TODO(tim): Bug 92948, 226464. ClientLogin is all but gone from use.

#ifndef SYNC_VIVALDI_SIGNIN_MANAGER_H_
#define SYNC_VIVALDI_SIGNIN_MANAGER_H_

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_internals_util.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/cookies/canonical_cookie.h"

class PrefService;
class ProfileOAuth2TokenService;
class SigninAccountIdHelper;
class SigninClient;

namespace vivaldi {

class VivaldiSigninManager : public SigninManagerBase,
                             public AccountTrackerService::Observer {
 public:
  // The callback invoked once the OAuth token has been fetched during signin,
  // but before the profile transitions to the "signed-in" state. This allows
  // callers to load policy and prompt the user appropriately before completing
  // signin. The callback is passed the just-fetched OAuth login refresh token.
  typedef base::Callback<void(const std::string&)> OAuthTokenFetchedCallback;

  // This is used to distinguish URLs belonging to the special web signin flow
  // running in the special signin process from other URLs on the same domain.
  // We do not grant WebUI privilieges / bindings to this process or to URLs of
  // this scheme; enforcement of privileges is handled separately by
  // OneClickSigninHelper.
  static const char kChromeSigninEffectiveSite[];

  VivaldiSigninManager(SigninClient* client,
                       AccountTrackerService* account_tracker_service);
  ~VivaldiSigninManager() override;

  // Sign a user out, removing the preference, erasing all keys
  // associated with the user, and canceling all auth in progress.
  void SignOut(signin_metrics::ProfileSignout signout_source_metric,
               signin_metrics::SignoutDelete signout_delete_metric) override;

  // On platforms where VivaldiSigninManager is responsible for dealing with
  // invalid username policy updates, we need to check this during
  // initialization and sign the user out.
  void Initialize(PrefService* local_state) override;
  void Shutdown() override;

  // Returns true if there's a signin in progress.
  bool AuthInProgress() const override;

  bool IsSigninAllowed() const override;

  // Returns true if the passed username is allowed by policy. Virtual for
  // mocking in tests.
  virtual bool IsAllowedUsername(const std::string& username) const;

  // If an authentication is in progress, return the username being
  // authenticated. Returns an empty string if no auth is in progress.
  const std::string& GetUsernameForAuthInProgress() const;

 private:
  enum SigninType { SIGNIN_TYPE_NONE, SIGNIN_TYPE_WITH_REFRESH_TOKEN };

  // Waits for the AccountTrackerService, then sends GoogleSigninSucceeded to
  // the client and clears the local password.
  void PostSignedIn();

  // AccountTrackerService::Observer implementation.
  void OnAccountUpdated(const AccountInfo& info) override;
  void OnAccountUpdateFailed(const std::string& account_id) override;

  // Called when a new request to re-authenticate a user is in progress.
  // Will clear in memory data but leaves the db as such so when the browser
  // restarts we can use the old token(which might throw a password error).
  void ClearTransientSigninData();

  // Called to handle an error from a GAIA auth fetch.  Sets the last error
  // to |error|, sends out a notification of login failure and clears the
  // transient signin data.
  void HandleAuthError(const GoogleServiceAuthError& error);

  void OnSigninAllowedPrefChanged();

  // ClientLogin identity.
  std::string password_;  // This is kept empty whenever possible.

  // The type of sign being performed.  This value is valid only between a call
  // to one of the StartSigninXXX methods and when the sign in is either
  // successful or not.
  SigninType type_;

  // The SigninClient object associated with this object. Must outlive this
  // object.
  SigninClient* client_;

  // The AccountTrackerService instance associated with this object. Must
  // outlive this object.
  AccountTrackerService* account_tracker_service_;

  // Helper object to listen for changes to signin preferences stored in non-
  // profile-specific local prefs (like kGoogleServicesUsernamePattern).
  PrefChangeRegistrar local_state_pref_registrar_;

  // Helper object to listen for changes to the signin allowed preference.
  BooleanPrefMember signin_allowed_;

  // Two gate conditions for when PostSignedIn should be called. Verify
  // that the VivaldiSigninManager has reached OnSignedIn() and the
  // AccountTracker
  // has completed calling GetUserInfo.
  bool signin_manager_signed_in_;
  bool user_info_fetched_by_account_tracker_;

  base::WeakPtrFactory<VivaldiSigninManager> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiSigninManager);
};
}  // namespace vivaldi
#endif  // SYNC_VIVALDI_SIGNIN_MANAGER_H_
