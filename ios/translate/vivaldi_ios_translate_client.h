// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TRANSLATE_VIVALDI_IOS_TRANSLATE_CLIENT_H_
#define IOS_TRANSLATE_VIVALDI_IOS_TRANSLATE_CLIENT_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <string>

#import "base/gtest_prod_util.h"
#import "base/memory/raw_ptr.h"
#import "components/translate/core/browser/translate_client.h"
#import "components/translate/core/browser/translate_step.h"
#import "components/translate/core/common/translate_errors.h"
#import "components/translate/ios/browser/ios_translate_driver.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

class PrefService;

namespace language {
class AcceptLanguagesService;
}

namespace translate {
class TranslatePrefs;
class TranslateManager;
class TranslateMetricsLogger;
}  // namespace translate

namespace web {
class WebState;
}  // namespace web

class VivaldiIOSTranslateClient
    : public translate::TranslateClient,
      public web::WebStateObserver,
      public web::WebStateUserData<VivaldiIOSTranslateClient> {
 public:
  VivaldiIOSTranslateClient(const VivaldiIOSTranslateClient&) = delete;
  VivaldiIOSTranslateClient& operator=(const VivaldiIOSTranslateClient&) = delete;

  ~VivaldiIOSTranslateClient() override;

  // Helper method to return a new TranslatePrefs instance.
  static std::unique_ptr<translate::TranslatePrefs> CreateTranslatePrefs(
      PrefService* prefs);

  // Gets the associated TranslateManager.
  translate::TranslateManager* GetTranslateManager();

  static void LoadTranslationScript();
  static std::string& GetTranslateScript();

  // TranslateClient implementation.
  translate::IOSTranslateDriver* GetTranslateDriver() override;
  PrefService* GetPrefs() override;
  std::unique_ptr<translate::TranslatePrefs> GetTranslatePrefs() override;
  language::AcceptLanguagesService* GetAcceptLanguagesService() override;
  std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      std::unique_ptr<translate::TranslateInfoBarDelegate> delegate)
      const override;
  bool ShowTranslateUI(translate::TranslateStep step,
                       const std::string& source_language,
                       const std::string& target_language,
                       translate::TranslateErrors error_type,
                       bool triggered_from_menu) override;
  bool IsTranslatableURL(const GURL& url) override;

 private:
  friend class web::WebStateUserData<VivaldiIOSTranslateClient>;
  FRIEND_TEST_ALL_PREFIXES(VivaldiIOSTranslateClient,
                           NewMetricsOnPageLoadCommits);
  FRIEND_TEST_ALL_PREFIXES(VivaldiIOSTranslateClient,
                           NoNewMetricsOnErrorPage);
  FRIEND_TEST_ALL_PREFIXES(VivaldiIOSTranslateClient,
                           PageTranslationCorrectlyUpdatesMetrics);

  explicit VivaldiIOSTranslateClient(web::WebState* web_state);

  // web::WebStateObserver implementation.
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Triggered when the foreground page is changed by a new navigation (in
  // DidStartNavigation) or is permanently closed (in WebStateDestroyed),
  // similar to PageLoadMetricsObserver::OnComplete.
  void DidPageLoadComplete();

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  raw_ptr<web::WebState> web_state_ = nullptr;

  translate::IOSTranslateDriver translate_driver_;
  std::unique_ptr<translate::TranslateManager> translate_manager_;

  // Metrics recorder for page load events.
  std::unique_ptr<translate::TranslateMetricsLogger> translate_metrics_logger_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_TRANSLATE_VIVALDI_IOS_TRANSLATE_CLIENT_H_
