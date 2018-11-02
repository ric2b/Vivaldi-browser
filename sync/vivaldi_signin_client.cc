// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/vivaldi_signin_client.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/local_auth.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_cookie_changed_subscription.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/signin/core/common/signin_switches.h"
#include "content/public/common/child_process_host.h"
#include "url/gurl.h"

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/first_run/first_run.h"
#endif

using content::RenderProcessHost;

VivaldiSigninClient::VivaldiSigninClient(
    Profile* profile,
    SigninErrorController* signin_error_controller)
    : profile_(profile), signin_error_controller_(signin_error_controller) {
  signin_error_controller_->AddObserver(this);
}

VivaldiSigninClient::~VivaldiSigninClient() {
  signin_error_controller_->RemoveObserver(this);
}

void VivaldiSigninClient::DoFinalInit() {}

PrefService* VivaldiSigninClient::GetPrefs() {
  return profile_->GetPrefs();
}

scoped_refptr<TokenWebData> VivaldiSigninClient::GetDatabase() {
  return WebDataServiceFactory::GetTokenWebDataForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
}

bool VivaldiSigninClient::CanRevokeCredentials() {
  // Don't allow revoking credentials for legacy supervised users.
  // See http://crbug.com/332032
  if (profile_->IsLegacySupervised()) {
    LOG(ERROR) << "Attempt to revoke supervised user refresh "
               << "token detected, ignoring.";
    return false;
  }
  return true;
}

std::string VivaldiSigninClient::GetSigninScopedDeviceId() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSigninScopedDeviceId)) {
    return std::string();
  }

  return SigninClient::GetOrCreateScopedDeviceIdPref(GetPrefs());
}

void VivaldiSigninClient::OnSignedOut() {
  ProfileAttributesEntry* entry;
  bool has_entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath(), &entry);

  // If sign out occurs because Sync setup was in progress and the Profile got
  // deleted, then the profile's no longer in the ProfileAttributesStorage.
  if (!has_entry)
    return;

  entry->SetLocalAuthCredentials(std::string());
  entry->SetAuthInfo(std::string(), base::string16());
  entry->SetIsSigninRequired(false);
}

net::URLRequestContextGetter* VivaldiSigninClient::GetURLRequestContext() {
  return profile_->GetRequestContext();
}

bool VivaldiSigninClient::ShouldMergeSigninCredentialsIntoCookieJar() {
  // If inline sign in is enabled, but account consistency is not, the user's
  // credentials should be merge into the cookie jar.
  return !signin::IsAccountConsistencyMirrorEnabled();
}

std::string VivaldiSigninClient::GetProductVersion() {
  return chrome::GetVersionString();
}

bool VivaldiSigninClient::IsFirstRun() const {
#if defined(OS_ANDROID)
  return false;
#else
  return first_run::IsChromeFirstRun();
#endif
}

base::Time VivaldiSigninClient::GetInstallDate() {
  return base::Time::FromTimeT(
      g_browser_process->metrics_service()->GetInstallDate());
}

std::unique_ptr<SigninClient::CookieChangedSubscription>
VivaldiSigninClient::AddCookieChangedCallback(
    const GURL& url,
    const std::string& name,
    const net::CookieStore::CookieChangedCallback& callback) {
  NOTREACHED();
  std::unique_ptr<SigninClient::CookieChangedSubscription> dummy;
  return dummy;
}

void VivaldiSigninClient::OnSignedIn(const std::string& account_id,
                                     const std::string& gaia_id,
                                     const std::string& username,
                                     const std::string& password) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesEntry* entry;
  if (profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath(), &entry)) {
    entry->SetAuthInfo(gaia_id, base::UTF8ToUTF16(username));
    ProfileMetrics::UpdateReportedProfilesStatistics(profile_manager);
  }
}

void VivaldiSigninClient::PostSignedIn(const std::string& account_id,
                                       const std::string& username,
                                       const std::string& password) {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // Don't store password hash except when lock is available for the user.
  if (!password.empty() && profiles::IsLockAvailable(profile_))
    LocalAuth::SetLocalAuthCredentials(profile_, password);
#endif
}

bool VivaldiSigninClient::AreSigninCookiesAllowed() {
  return false;
}

void VivaldiSigninClient::OnErrorChanged() {
  // Some tests don't have a ProfileManager.
  if (g_browser_process->profile_manager() == nullptr)
    return;

  ProfileAttributesEntry* entry;

  if (!g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile_->GetPath(), &entry)) {
    return;
  }

  entry->SetIsAuthError(signin_error_controller_->HasError());
}

void VivaldiSigninClient::DelayNetworkCall(const base::Closure& callback) {
  // Don't bother if we don't have any kind of network connection.
  if (net::NetworkChangeNotifier::IsOffline()) {
    delayed_callbacks_.push_back(callback);
  } else {
    callback.Run();
  }
}

std::unique_ptr<GaiaAuthFetcher> VivaldiSigninClient::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    const std::string& source,
    net::URLRequestContextGetter* getter) {
  return base::MakeUnique<GaiaAuthFetcher>(consumer, source, getter);
}

void VivaldiSigninClient::AddContentSettingsObserver(
    content_settings::Observer* observer) {
  HostContentSettingsMapFactory::GetForProfile(profile_)->AddObserver(observer);
}

void VivaldiSigninClient::RemoveContentSettingsObserver(
    content_settings::Observer* observer) {
  HostContentSettingsMapFactory::GetForProfile(profile_)->RemoveObserver(
      observer);
}
