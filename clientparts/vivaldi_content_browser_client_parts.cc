// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#include "clientparts/vivaldi_content_browser_client_parts.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/platform_locale_settings.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/helper/vivaldi_app_helper.h"
#endif

namespace vivaldi {

bool HandleVivaldiURLRewrite(GURL* url,
                             content::BrowserContext* browser_context) {
  // Don't touch non-vivaldi schemes
  if (!url->SchemeIs(vivaldi::kVivaldiUIScheme))
    return false;

  GURL::Replacements replacements;
  replacements.SetSchemeStr(content::kChromeUIScheme);
  *url = url->ReplaceComponents(replacements);

  return false;
}
}  // namespace vivaldi

void VivaldiContentBrowserClientParts::BrowserURLHandlerCreated(
    content::BrowserURLHandler* handler) {
  // rewrite vivaldi: links to long links, and reverse
  // TODO(pettern): Enable later when the js rewrites are gone
  handler->AddHandlerPair(&vivaldi::HandleVivaldiURLRewrite,
                          content::BrowserURLHandler::null_handler());
}

void VivaldiContentBrowserClientParts::OverrideWebkitPrefs(
    content::RenderViewHost* rvh,
    content::WebPreferences* web_prefs) {
  if (!vivaldi::IsVivaldiRunning())
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  if (web_contents) {
    // web_contents is nullptr on interstitial pages.
#if !defined(OS_ANDROID)
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    PrefService* prefs = profile->GetPrefs();
    web_prefs->tabs_to_links =
        prefs->GetBoolean(vivaldiprefs::kWebpagesTabFocusesLinks);
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
    extensions::VivaldiAppHelper* vivaldi_app_helper =
        extensions::VivaldiAppHelper::FromWebContents(web_contents);
    // Returns nullptr on regular pages, and a valid VivaldiAppHelper
    // for the WebContents used for our UI, so it's safe to use to check
    // whether we're the UI or not.
    if (vivaldi_app_helper) {
      int min_font_size = 0;
      bool success = base::StringToInt(
          l10n_util::GetStringUTF8(IDS_MINIMUM_FONT_SIZE), &min_font_size);
      if (success)
        web_prefs->minimum_font_size = min_font_size;
    }
#endif
  }
}
