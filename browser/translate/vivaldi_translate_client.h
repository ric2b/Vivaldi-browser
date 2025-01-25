// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BROWSER_TRANSLATE_VIVALDI_TRANSLATE_CLIENT_H_
#define BROWSER_TRANSLATE_VIVALDI_TRANSLATE_CLIENT_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/schema/tabs_private.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

class PrefService;

namespace translate {
class AutoTranslateSnackbarController;
class LanguageState;
class TranslateAcceptLanguages;
class TranslatePrefs;
class TranslateManager;
class TranslateMessage;

struct LanguageDetectionDetails;
}  // namespace translate

#if BUILDFLAG(ENABLE_EXTENSIONS)
using extensions::vivaldi::tabs_private::TranslateError;
using extensions::vivaldi::tabs_private::TranslateStep;

TranslateStep ToVivaldiTranslateStep(translate::TranslateStep step);
TranslateError ToVivaldiTranslateError(translate::TranslateErrors error);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

class VivaldiTranslateClient
    : public translate::TranslateClient,
      public translate::ContentTranslateDriver::LanguageDetectionObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<VivaldiTranslateClient> {
 public:
  ~VivaldiTranslateClient() override;
  VivaldiTranslateClient(const VivaldiTranslateClient&) = delete;
  VivaldiTranslateClient& operator=(const VivaldiTranslateClient&) = delete;

  // Gets the LanguageState associated with the page.
  const translate::LanguageState& GetLanguageState();

  // Returns the ContentTranslateDriver instance associated with this
  // WebContents.
  translate::ContentTranslateDriver* translate_driver();

  // Helper method to return a new TranslatePrefs instance.
  static std::unique_ptr<translate::TranslatePrefs> CreateTranslatePrefs(
      PrefService* prefs);

  // Helper method to return the TranslateManager instance associated with
  // |web_contents|, or NULL if there is no such associated instance.
  static translate::TranslateManager* GetManagerFromWebContents(
      content::WebContents* web_contents);

  static void LoadTranslationScript();
  static std::string& GetTranslateScript();

  // Gets |source| and |target| language for translation.
  void GetTranslateLanguages(content::WebContents* web_contents,
                             std::string* source,
                             std::string* target);

  // Gets the associated TranslateManager.
  translate::TranslateManager* GetTranslateManager();

  // TranslateClient implementation.
  translate::TranslateDriver* GetTranslateDriver() override;
  PrefService* GetPrefs() override;
  std::unique_ptr<translate::TranslatePrefs> GetTranslatePrefs() override;
  language::AcceptLanguagesService* GetAcceptLanguagesService() override;

  bool ShowTranslateUI(translate::TranslateStep step,
                       const std::string& source_language,
                       const std::string& target_language,
                       translate::TranslateErrors error_type,
                       bool triggered_from_menu) override;
  bool IsTranslatableURL(const GURL& url) override;

  // TranslateDriver::LanguageDetectionObserver implementation.
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;

#if BUILDFLAG(IS_ANDROID)
  // Trigger a manual translation when the necessary state (e.g. source
  // language) is ready.
  void ManualTranslateWhenReady();
#endif
  void SetPredefinedTargetLanguage(const std::string& translate_language_code);

 private:
  explicit VivaldiTranslateClient(content::WebContents* web_contents);
  friend class content::WebContentsUserData<VivaldiTranslateClient>;

  // content::WebContentsObserver implementation.
  void WebContentsDestroyed() override;

  std::unique_ptr<translate::ContentTranslateDriver> translate_driver_;
  std::unique_ptr<translate::TranslateManager> translate_manager_;

#if BUILDFLAG(IS_ANDROID)
  // Whether to trigger a manual translation when ready.
  // See ChromeTranslateClient::ManualTranslateOnReady
  bool manual_translate_on_ready_ = false;

  std::unique_ptr<translate::TranslateMessage> translate_message_;
  std::unique_ptr<translate::AutoTranslateSnackbarController>
      auto_translate_snackbar_controller_;

  // content::WebContentsObserver implementation on Android only. Used for the
  // auto-translate Snackbar.
  void PrimaryPageChanged(content::Page& page) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
#endif

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // BROWSER_TRANSLATE_VIVALDI_TRANSLATE_CLIENT_H_
