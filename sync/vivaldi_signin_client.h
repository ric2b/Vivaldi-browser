// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_VIVALDI_SIGNIN_CLIENT_H_
#define SYNC_VIVALDI_SIGNIN_CLIENT_H_

#include <list>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "content/public/browser/render_process_host_observer.h"

class CookieSettings;
class Profile;

class VivaldiSigninClient : public SigninClient,
                            public SigninErrorController::Observer {
 public:
  explicit VivaldiSigninClient(Profile* profile,
                               SigninErrorController* signin_error_controller);
  ~VivaldiSigninClient() override;

  // SigninClient implementation.
  void DoFinalInit() override;
  PrefService* GetPrefs() override;
  scoped_refptr<TokenWebData> GetDatabase() override;
  bool CanRevokeCredentials() override;
  std::string GetSigninScopedDeviceId() override;
  void OnSignedOut() override;
  net::URLRequestContextGetter* GetURLRequestContext() override;
  bool ShouldMergeSigninCredentialsIntoCookieJar() override;
  bool IsFirstRun() const override;
  base::Time GetInstallDate() override;

  // Returns a string describing the chrome version environment. Version format:
  // <Build Info> <OS> <Version number> (<Last change>)<channel or "-devel">
  // If version information is unavailable, returns "invalid."
  std::string GetProductVersion() override;
  std::unique_ptr<CookieChangedSubscription> AddCookieChangedCallback(
      const GURL& url,
      const std::string& name,
      const net::CookieStore::CookieChangedCallback& callback) override;
  void OnSignedIn(const std::string& account_id,
                  const std::string& gaia_id,
                  const std::string& username,
                  const std::string& password) override;
  void PostSignedIn(const std::string& account_id,
                    const std::string& username,
                    const std::string& password) override;

  // Returns true if GAIA cookies are allowed in the content area.
  bool AreSigninCookiesAllowed() override;

  // Adds an observer to listen for changes to the state of sign in cookie
  // settings.
  void AddContentSettingsObserver(
      content_settings::Observer* observer) override;
  void RemoveContentSettingsObserver(
      content_settings::Observer* observer) override;

  // SigninErrorController::Observer implementation.
  void OnErrorChanged() override;

  // Execute |callback| if and when there is a network connection.
  void DelayNetworkCall(const base::Closure& callback) override;

  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      const std::string& source,
      net::URLRequestContextGetter* getter) override;

 private:
  Profile* profile_;

  SigninErrorController* signin_error_controller_;
  std::list<base::Closure> delayed_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiSigninClient);
};

#endif  // SYNC_VIVALDI_SIGNIN_CLIENT_H_
