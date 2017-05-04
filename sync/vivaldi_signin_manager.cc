// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/vivaldi_signin_manager.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_internals_util.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/escape.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

using namespace signin_internals_util;

namespace vivaldi {

// Under the covers, we use a dummy chrome-extension ID to serve the purposes
// outlined in the .h file comment for this string.
const char VivaldiSigninManager::kChromeSigninEffectiveSite[] = "";

VivaldiSigninManager::VivaldiSigninManager(
    SigninClient* client,
    AccountTrackerService* account_tracker_service)
    : SigninManagerBase(client, account_tracker_service),
      type_(SIGNIN_TYPE_NONE),
      client_(client),
      signin_manager_signed_in_(false),
      user_info_fetched_by_account_tracker_(false),
      weak_pointer_factory_(this) {}

VivaldiSigninManager::~VivaldiSigninManager() {}

void VivaldiSigninManager::ClearTransientSigninData() {
  DCHECK(IsInitialized());

  type_ = SIGNIN_TYPE_NONE;
}

void VivaldiSigninManager::HandleAuthError(
    const GoogleServiceAuthError& error) {
  ClearTransientSigninData();

  for (auto& observer : observer_list_)
    observer.GoogleSigninFailed(error);
}

void VivaldiSigninManager::SignOut(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  DCHECK(IsInitialized());

  signin_metrics::LogSignout(signout_source_metric, signout_delete_metric);
  if (!IsAuthenticated()) {
    if (AuthInProgress()) {
      // If the user is in the process of signing in, then treat a call to
      // SignOut as a cancellation request.
      GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
      HandleAuthError(error);
    } else {
      // Clean up our transient data and exit if we aren't signed in.
      // This avoids a perf regression from clearing out the TokenDB if
      // SignOut() is invoked on startup to clean up any incomplete previous
      // signin attempts.
      ClearTransientSigninData();
    }
    return;
  }

  ClearTransientSigninData();

  const std::string account_id = GetAuthenticatedAccountId();
  const std::string username = GetAuthenticatedAccountInfo().email;
  const base::Time signin_time = base::Time::FromInternalValue(
      client_->GetPrefs()->GetInt64(prefs::kSignedInTime));
  client_->GetPrefs()->ClearPref(prefs::kGoogleServicesHostedDomain);
  client_->GetPrefs()->ClearPref(prefs::kGoogleServicesAccountId);
  client_->GetPrefs()->ClearPref(prefs::kGoogleServicesUserAccountId);
  client_->GetPrefs()->ClearPref(prefs::kSignedInTime);
  client_->SignOut();

  // Erase (now) stale information from AboutSigninInternals.
  NotifyDiagnosticsObservers(SIGNIN_COMPLETED, "");

  // Determine the duration the user was logged in and log that to UMA.
  if (!signin_time.is_null()) {
    base::TimeDelta signed_in_duration = base::Time::Now() - signin_time;
    UMA_HISTOGRAM_COUNTS("Signin.SignedInDurationBeforeSignout",
                         signed_in_duration.InMinutes());
  }

  for (auto& observer : observer_list_)
    observer.GoogleSignedOut(account_id, username);
}

void VivaldiSigninManager::Initialize(PrefService* local_state) {
  SigninManagerBase::Initialize(local_state);

  // local_state can be null during unit tests.
  if (local_state) {
    local_state_pref_registrar_.Init(local_state);
  }
  DCHECK(client_->GetPrefs());
  signin_allowed_.Init(
      prefs::kSigninAllowed, client_->GetPrefs(),
      base::Bind(&VivaldiSigninManager::OnSigninAllowedPrefChanged,
                 base::Unretained(this)));

  std::string user =
      client_->GetPrefs()->GetString(prefs::kGoogleServicesUsername);
  if ((!user.empty() && !IsAllowedUsername(user)) || !IsSigninAllowed()) {
    // User is signed in, but the username is invalid - the administrator must
    // have changed the policy since the last signin, so sign out the user.
    SignOut(signin_metrics::SIGNIN_PREF_CHANGED_DURING_SIGNIN,
            signin_metrics::SignoutDelete::IGNORE_METRIC);
  }
}

void VivaldiSigninManager::Shutdown() {
  local_state_pref_registrar_.RemoveAll();
  SigninManagerBase::Shutdown();
}

bool VivaldiSigninManager::IsSigninAllowed() const {
  return signin_allowed_.GetValue();
}

bool VivaldiSigninManager::AuthInProgress() const {
  return false;
}

void VivaldiSigninManager::OnSigninAllowedPrefChanged() {
  if (!IsSigninAllowed())
    SignOut(signin_metrics::SIGNOUT_PREF_CHANGED,
            signin_metrics::SignoutDelete::IGNORE_METRIC);
}

bool VivaldiSigninManager::IsAllowedUsername(
    const std::string& username) const {
  return true;
}

void VivaldiSigninManager::PostSignedIn() {
  if (!signin_manager_signed_in_ || !user_info_fetched_by_account_tracker_)
    return;

  client_->PostSignedIn(GetAuthenticatedAccountId(),
                        GetAuthenticatedAccountInfo().email, password_);
}

void VivaldiSigninManager::OnAccountUpdated(const AccountInfo& info) {
  user_info_fetched_by_account_tracker_ = true;
  PostSignedIn();
}

void VivaldiSigninManager::OnAccountUpdateFailed(
    const std::string& account_id) {
  user_info_fetched_by_account_tracker_ = true;
  PostSignedIn();
}
}
