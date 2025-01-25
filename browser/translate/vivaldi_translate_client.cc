// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser/translate/vivaldi_translate_client.h"

#include <string>
#include <string_view>

#include "app/vivaldi_resources.h"
#include "apps/switches.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/vivaldi_switches.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/language/accept_languages_service_factory.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/translate/translate_ranker_factory.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/translate/translate_bubble_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/theme_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_util.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/tools/vivaldi_tools.h"
#include "third_party/metrics_proto/translate_event.pb.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/translate/android/auto_translate_snackbar_controller.h"
#include "components/translate/content/android/translate_message.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

VivaldiTranslateClient::VivaldiTranslateClient(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<VivaldiTranslateClient>(*web_contents) {
  translate_driver_ = std::make_unique<translate::ContentTranslateDriver>(
      *web_contents,
      UrlLanguageHistogramFactory::GetForBrowserContext(
          web_contents->GetBrowserContext()));
  translate_manager_ = std::make_unique<translate::TranslateManager>(
      this,
      translate::TranslateRankerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext()),
      LanguageModelManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())
          ->GetPrimaryModel());
  if (translate_driver_) {
    translate_driver_->AddLanguageDetectionObserver(this);
    translate_driver_->set_translate_manager(translate_manager_.get());
  }
  std::string script = VivaldiTranslateClient::GetTranslateScript();
  DCHECK(!script.empty());
  translate_manager_->SetTranslationScript(script);
  // We don't want API checks when using our own servers.
  translate::TranslateManager::SetIgnoreMissingKeyForTesting(true);
}

VivaldiTranslateClient::~VivaldiTranslateClient() {
  if (translate_driver_) {
    translate_driver_->RemoveLanguageDetectionObserver(this);
    translate_driver_->set_translate_manager(nullptr);
  }
}

/* static */
std::string& VivaldiTranslateClient::GetTranslateScript() {
  static base::NoDestructor<std::string> translate_script;
  return *translate_script;
}

#if !BUILDFLAG(IS_ANDROID)
constexpr base::FilePath::StringPieceType kTranslateBundleName(
    FILE_PATH_LITERAL("translate-bundle.js"));
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {
std::string ReplaceServerUrl(std::string& script) {
  const std::string search_pattern = "$OVERRIDE_TRANSLATE_SERVER";
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  std::string server =
      cmd_line.GetSwitchValueASCII(switches::kTranslateServerUrl);

  base::ReplaceSubstringsAfterOffset(&script, 0, search_pattern, server);

  return script;
}
#if BUILDFLAG(IS_ANDROID)
// helper function for use in VivaldiTranslateClient::ShowTranslateUI.
bool IsAutomaticTranslationType(translate::TranslationType type) {
  return type == translate::TranslationType::kAutomaticTranslationByHref ||
         type == translate::TranslationType::kAutomaticTranslationByLink ||
         type == translate::TranslationType::kAutomaticTranslationByPref ||
         type == translate::TranslationType::
                     kAutomaticTranslationToPredefinedTarget;
}
#endif
}  // namespace

/* static */
void VivaldiTranslateClient::LoadTranslationScript() {
#if !BUILDFLAG(IS_ANDROID)
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(apps::kLoadAndLaunchApp)) {
    base::CommandLine::StringType path =
        command_line.GetSwitchValueNative(apps::kLoadAndLaunchApp);
    base::FilePath filepath(path);

    filepath = filepath.Append(kTranslateBundleName);
    if (base::PathExists(filepath)) {
      std::string script;
      if (base::ReadFileToString(filepath, &script)) {
        DCHECK(!script.empty());
        VivaldiTranslateClient::GetTranslateScript() = ReplaceServerUrl(script);
      }
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  if (VivaldiTranslateClient::GetTranslateScript().empty()) {
    std::string script =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            VIVALDI_TRANSLATE_JS);
    DCHECK(!script.empty());
    VivaldiTranslateClient::GetTranslateScript() = ReplaceServerUrl(script);
  }
}

const translate::LanguageState& VivaldiTranslateClient::GetLanguageState() {
  return *translate_manager_->GetLanguageState();
}

translate::ContentTranslateDriver* VivaldiTranslateClient::translate_driver() {
  if (translate_driver_) {
    return translate_driver_.get();
  }
  return nullptr;
}

std::unique_ptr<translate::TranslatePrefs>
VivaldiTranslateClient::CreateTranslatePrefs(PrefService* prefs) {
  return ChromeTranslateClient::CreateTranslatePrefs(prefs);
}

language::AcceptLanguagesService*
VivaldiTranslateClient::GetAcceptLanguagesService() {
  DCHECK(web_contents());
  return AcceptLanguagesServiceFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
}

// static
translate::TranslateManager* VivaldiTranslateClient::GetManagerFromWebContents(
    content::WebContents* web_contents) {
  VivaldiTranslateClient* translate_client = FromWebContents(web_contents);
  if (!translate_client) {
    return nullptr;
  }
  return translate_client->GetTranslateManager();
}

void VivaldiTranslateClient::GetTranslateLanguages(
    content::WebContents* web_contents,
    std::string* source,
    std::string* target) {
  DCHECK(source != NULL);
  DCHECK(target != NULL);

  *source = translate::TranslateDownloadManager::GetLanguageCode(
      GetLanguageState().source_language());

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      CreateTranslatePrefs(profile->GetPrefs());
  if (!profile->IsOffTheRecord()) {
    std::string auto_translate_language =
        translate::TranslateManager::GetAutoTargetLanguage(
            *source, translate_prefs.get());
    if (!auto_translate_language.empty()) {
      *target = auto_translate_language;
      return;
    }
  }

  *target = translate::TranslateManager::GetTargetLanguage(
      translate_prefs.get(), LanguageModelManagerFactory::GetInstance()
                                 ->GetForBrowserContext(profile)
                                 ->GetPrimaryModel());
}

translate::TranslateManager* VivaldiTranslateClient::GetTranslateManager() {
  return translate_manager_.get();
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
using extensions::vivaldi::tabs_private::TranslateError;
using extensions::vivaldi::tabs_private::TranslateStep;

TranslateStep ToVivaldiTranslateStep(translate::TranslateStep step) {
  switch (step) {
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE:
      return TranslateStep::kBeforeTranslate;
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATING:
      return TranslateStep::kTranslating;
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE:
      return TranslateStep::kAfterTranslate;
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR:
      return TranslateStep::kTranslateError;
    default:
      NOTREACHED();
      //return TranslateStep::kIdle;
  }
}

TranslateError ToVivaldiTranslateError(translate::TranslateErrors error) {
  switch (error) {
    case translate::TranslateErrors::NONE:
      return TranslateError::kNoError;
    case translate::TranslateErrors::NETWORK:
      return TranslateError::kNetwork;
    case translate::TranslateErrors::INITIALIZATION_ERROR:
      return TranslateError::kInitError;
    case translate::TranslateErrors::UNKNOWN_LANGUAGE:
      return TranslateError::kUnknownLanguage;
    case translate::TranslateErrors::UNSUPPORTED_LANGUAGE:
      return TranslateError::kUnsupportedLanguage;
    case translate::TranslateErrors::IDENTICAL_LANGUAGES:
      return TranslateError::kIdenticalLanguages;
    case translate::TranslateErrors::TRANSLATION_ERROR:
      return TranslateError::kTranslationError;
    case translate::TranslateErrors::TRANSLATION_TIMEOUT:
      return TranslateError::kTranslationTimeout;
    case translate::TranslateErrors::UNEXPECTED_SCRIPT_ERROR:
      return TranslateError::kUnexpectedScriptError;
    case translate::TranslateErrors::BAD_ORIGIN:
      return TranslateError::kBadOrigin;
    case translate::TranslateErrors::SCRIPT_LOAD_ERROR:
      return TranslateError::kScriptLoadError;
    default:
      NOTREACHED();
      //return TranslateError::kNone;
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

bool VivaldiTranslateClient::ShowTranslateUI(
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors error_type,
    bool triggered_from_menu /* only set on android */) {
  DCHECK(web_contents());
  DCHECK(translate_manager_);

  if (error_type != translate::TranslateErrors::NONE) {
    step = translate::TRANSLATE_STEP_TRANSLATE_ERROR;
  }

// Translate uses a an SnackBar on Android (here)
#if BUILDFLAG(IS_ANDROID)
  DCHECK(!TranslateService::IsTranslateBubbleEnabled());
  // Message UI.
  translate::TranslationType translate_type =
          GetLanguageState().translation_type();
  // Use the automatic translation Snackbar if the current translation is an
  // automatic translation and there was no error.
  if (IsAutomaticTranslationType(translate_type) &&
      step != translate::TRANSLATE_STEP_TRANSLATE_ERROR) {
      // The Automatic translation snackbar is only shown after translation
      // has completed. The translating step is a no-op with the Snackbar.
      if (step == translate::TRANSLATE_STEP_AFTER_TRANSLATE) {

        // An automatic translation has completed show the snackbar.
        if (!auto_translate_snackbar_controller_) {
          auto_translate_snackbar_controller_ =
              std::make_unique<translate::AutoTranslateSnackbarController>(
                  web_contents(), translate_manager_->GetWeakPtr());
        }
        auto_translate_snackbar_controller_->ShowSnackbar(target_language);
      }
    } else {
      // Not an automatic translation. Use TranslateMessage instead.
      if (!translate_message_) {
        translate_message_ = std::make_unique<translate::TranslateMessage>(
            web_contents(), translate_manager_->GetWeakPtr(),
            base::BindRepeating([]() {}));
      }
      translate_message_->ShowTranslateStep(step, source_language,
                                            target_language);
    }
  translate_manager_->GetActiveTranslateMetricsLogger()->LogUIChange(true);
#else
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::vivaldi::tabs_private::TranslateError api_error =
      ToVivaldiTranslateError(error_type);
  extensions::vivaldi::tabs_private::TranslateStep api_step =
      ToVivaldiTranslateStep(step);

  bool auto_translate = false;
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      CreateTranslatePrefs(profile->GetPrefs());
  if (!profile->IsOffTheRecord()) {
    std::string source = translate::TranslateDownloadManager::GetLanguageCode(
        GetLanguageState().source_language());
    std::string auto_translate_language =
        translate::TranslateManager::GetAutoTargetLanguage(
            source, translate_prefs.get());
    if (!auto_translate_language.empty()) {
      auto_translate = true;
    }
  }
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents()).id();
  if (tab_id) {
    ::vivaldi::BroadcastEvent(
        extensions::vivaldi::tabs_private::OnShowTranslationUI::kEventName,
        extensions::vivaldi::tabs_private::OnShowTranslationUI::Create(
            tab_id, api_step, api_error, auto_translate),
        web_contents()->GetBrowserContext());
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#endif  // IS_ANDROID

  return true;
}

translate::TranslateDriver* VivaldiTranslateClient::GetTranslateDriver() {
  return translate_driver();
}

PrefService* VivaldiTranslateClient::GetPrefs() {
  DCHECK(web_contents());
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return profile->GetPrefs();
}

std::unique_ptr<translate::TranslatePrefs>
VivaldiTranslateClient::GetTranslatePrefs() {
  return CreateTranslatePrefs(GetPrefs());
}

#if BUILDFLAG(IS_ANDROID)
void VivaldiTranslateClient::ManualTranslateWhenReady() {
  if (GetLanguageState().source_language().empty()) {
    manual_translate_on_ready_ = true;
  } else {
    translate::TranslateManager* manager = GetTranslateManager();
#if BUILDFLAG(IS_ANDROID)
    manager->ShowTranslateUI(true, true);
#else
    manager->ShowTranslateUI(true);
#endif
  }
}
#endif

void VivaldiTranslateClient::SetPredefinedTargetLanguage(
    const std::string& translate_language_code) {
  translate::TranslateManager* manager = GetTranslateManager();
  manager->SetPredefinedTargetLanguage(translate_language_code);
}

bool VivaldiTranslateClient::IsTranslatableURL(const GURL& url) {
  return TranslateService::IsTranslatableURL(url);
}

void VivaldiTranslateClient::WebContentsDestroyed() {
  // Translation process can be interrupted.
  // Destroying the TranslateManager now guarantees that it never has to deal
  // with NULL WebContents.
    if (translate_manager_) {
        if (translate_driver_) {
            translate_driver_->set_translate_manager(nullptr);
        }
        translate_manager_.reset();
    }
}

// ContentTranslateDriver::TranslationObserver implementation.
void VivaldiTranslateClient::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  translate::TranslateBrowserMetrics::ReportLanguageDetectionContentLength(
      details.contents.size());

  if (!web_contents()->GetBrowserContext()->IsOffTheRecord() &&
      IsTranslatableURL(details.url)) {
    GetTranslateManager()->NotifyLanguageDetected(details);
  }

#if BUILDFLAG(IS_ANDROID)
  // See ChromeTranslateClient::ManualTranslateOnReady
  if (manual_translate_on_ready_) {
    GetTranslateManager()->ShowTranslateUI(true);
    manual_translate_on_ready_ = false;
  }
#endif
}
#if BUILDFLAG(IS_ANDROID)
void VivaldiTranslateClient::PrimaryPageChanged(content::Page& page) {
  if (auto_translate_snackbar_controller_ &&
      auto_translate_snackbar_controller_->IsShowing()) {
    auto_translate_snackbar_controller_->NativeDismissSnackbar();
  }
}

void VivaldiTranslateClient::OnVisibilityChanged(
    content::Visibility visibility) {
  if (auto_translate_snackbar_controller_ &&
      auto_translate_snackbar_controller_->IsShowing() &&
      visibility == content::Visibility::HIDDEN) {
    auto_translate_snackbar_controller_->NativeDismissSnackbar();
  }
}
#endif
// The bubble is implemented only on the desktop platforms.
#if !BUILDFLAG(IS_ANDROID)
#if 0
ShowTranslateBubbleResult VivaldiTranslateClient::ShowBubble(
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors error_type) {
  DCHECK(translate_manager_);
  Browser* browser = chrome::FindBrowserWithTab(web_contents());

  // |browser| might be NULL when testing. In this case, Show(...) should be
  // called because the implementation for testing is used.
  if (!browser) {
    return TranslateBubbleFactory::Show(NULL, web_contents(), step,
                                        source_language, target_language,
                                        error_type);
  }

  if (web_contents() != browser->tab_strip_model()->GetActiveWebContents())
    return ShowTranslateBubbleResult::WEB_CONTENTS_NOT_ACTIVE;

  // This ShowBubble function is also used for updating the existing bubble.
  // However, with the bubble shown, any browser windows are NOT activated
  // because the bubble takes the focus from the other widgets including the
  // browser windows. So it is checked that |browser| is the last activated
  // browser, not is now activated.
  if (browser != chrome::FindLastActive())
    return ShowTranslateBubbleResult::BROWSER_WINDOW_NOT_ACTIVE;

  // During auto-translating, the bubble should not be shown.
  if (step == translate::TRANSLATE_STEP_TRANSLATING ||
      step == translate::TRANSLATE_STEP_AFTER_TRANSLATE) {
    if (GetLanguageState().InTranslateNavigation())
      return ShowTranslateBubbleResult::SUCCESS;
  }

  return TranslateBubbleFactory::Show(browser->window(), web_contents(), step,
                                      source_language, target_language,
                                      error_type);
}
#endif
#endif

WEB_CONTENTS_USER_DATA_KEY_IMPL(VivaldiTranslateClient);
