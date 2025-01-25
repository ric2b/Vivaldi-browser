// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_pages.h"
#include "chromeos/crosapi/mojom/feedback.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"

namespace chrome {
namespace internal {

namespace {

crosapi::mojom::LacrosFeedbackSource ToMojoLacrosFeedbackSource(
    feedback::FeedbackSource source) {
  switch (source) {
    case feedback::kFeedbackSourceBrowserCommand:
      return crosapi::mojom::LacrosFeedbackSource::kLacrosBrowserCommand;
    case feedback::kFeedbackSourceMdSettingsAboutPage:
      return crosapi::mojom::LacrosFeedbackSource::kLacrosSettingsAboutPage;
    case feedback::kFeedbackSourceAutofillContextMenu:
      return crosapi::mojom::LacrosFeedbackSource::kLacrosAutofillContextMenu;
    case feedback::kFeedbackSourceSadTabPage:
      return crosapi::mojom::LacrosFeedbackSource::kLacrosSadTabPage;
    case feedback::kFeedbackSourceChromeLabs:
      return crosapi::mojom::LacrosFeedbackSource::kLacrosChromeLabs;
    case feedback::kFeedbackSourceQuickAnswers:
      return crosapi::mojom::LacrosFeedbackSource::kLacrosQuickAnswers;
    case feedback::kFeedbackSourceWindowLayoutMenu:
      return crosapi::mojom::LacrosFeedbackSource::
          kDeprecatedLacrosWindowLayoutMenu;
    case feedback::kFeedbackSourceCookieControls:
      return crosapi::mojom::LacrosFeedbackSource::
          kFeedbackSourceCookieControls;
    case feedback::kFeedbackSourceSettingsPerformancePage:
      return crosapi::mojom::LacrosFeedbackSource::
          kFeedbackSourceSettingsPerformancePage;
    case feedback::kFeedbackSourceProfileErrorDialog:
      return crosapi::mojom::LacrosFeedbackSource::
          kFeedbackSourceProfileErrorDialog;
    case feedback::kFeedbackSourceQuickOffice:
      return crosapi::mojom::LacrosFeedbackSource::kFeedbackSourceQuickOffice;
    case feedback::kFeedbackSourceAI:
      return crosapi::mojom::LacrosFeedbackSource::kFeedbackSourceAI;
    case feedback::kFeedbackSourceLensOverlay:
      return crosapi::mojom::LacrosFeedbackSource::kFeedbackSourceLensOverlay;
    default:
      LOG(ERROR) << "ShowFeedbackPage is called by unknown Lacros source: "
                 << static_cast<int>(source);
      NOTREACHED_IN_MIGRATION();
      return crosapi::mojom::LacrosFeedbackSource::kUnknown;
  }
}

crosapi::mojom::FeedbackInfoPtr ToMojoFeedbackInfo(
    const GURL& page_url,
    feedback::FeedbackSource source,
    const std::string& description_template,
    const std::string& description_placeholder_text,
    const std::string& category_tag,
    const std::string& extra_diagnostics,
    base::Value::Dict autofill_metadata,
    base::Value::Dict ai_metadata) {
  auto mojo_feedback = crosapi::mojom::FeedbackInfo::New();
  mojo_feedback->page_url = page_url;
  mojo_feedback->source = ToMojoLacrosFeedbackSource(source);
  mojo_feedback->description_template = description_template;
  mojo_feedback->description_placeholder_text = description_placeholder_text;
  mojo_feedback->category_tag = category_tag;
  mojo_feedback->extra_diagnostics = extra_diagnostics;
  mojo_feedback->autofill_metadata = base::Value(std::move(autofill_metadata));
  mojo_feedback->ai_metadata = base::Value(std::move(ai_metadata));
  return mojo_feedback;
}

}  //  namespace

// Requests to show Feedback ui remotely in ash via crosapi mojo call.
// Note: This function should only be called from show_feedback_page.cc.
void ShowFeedbackPageLacros(const GURL& page_url,
                            feedback::FeedbackSource source,
                            const std::string& description_template,
                            const std::string& description_placeholder_text,
                            const std::string& category_tag,
                            const std::string& extra_diagnostics,
                            base::Value::Dict autofill_metadata,
                            base::Value::Dict ai_metadata) {
  if (source == feedback::kFeedbackSourceAI) {
    auto capabilities = chromeos::BrowserParamsProxy::Get()->AshCapabilities();
    if (!capabilities || !base::Contains(*capabilities, "crbug/1501057")) {
      LOG(WARNING) << "Unsupported feedback source AI for ash.";
      return;
    }
  }

  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::Feedback>()
      ->ShowFeedbackPage(ToMojoFeedbackInfo(
          page_url, source, description_template, description_placeholder_text,
          category_tag, extra_diagnostics, std::move(autofill_metadata),
          std::move(ai_metadata)));
}

}  // namespace internal
}  // namespace chrome
