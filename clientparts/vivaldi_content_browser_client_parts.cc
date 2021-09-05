// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#include "clientparts/vivaldi_content_browser_client_parts.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/platform_locale_settings.h"
#include "components/prefs/pref_service.h"
#include "components/request_filter/request_filter_manager.h"
#include "components/request_filter/request_filter_manager_factory.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/api/features/vivaldi_runtime_feature.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "ui/base/l10n/l10n_util.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/api/runtime/runtime_api.h"
#include "extensions/helper/vivaldi_app_helper.h"
#endif

namespace vivaldi {

bool HandleVivaldiURLRewrite(GURL* url,
                             content::BrowserContext* browser_context) {
  if (url->SchemeIs(vivaldi::kVivaldiUIScheme)) {
    // If we get here, it means a vivaldi: url not handled in js was
    // entered. Since we do not have vivaldi: as a valid scheme in chromium,
    // we change it to chrome:
    GURL::Replacements replacements;
    replacements.SetSchemeStr(content::kChromeUIScheme);
    *url = url->ReplaceComponents(replacements);
    return true;
  }
  // Rewrite chrome://newtab
  if (url->SchemeIs(content::kChromeUIScheme) &&
      url->host() == chrome::kChromeUINewTabHost) {
    // Rewrite the url to our startpage.
    *url = GURL(kVivaldiNewTabURL);
    return true;
  }
  return false;
}

}  // namespace vivaldi

void VivaldiContentBrowserClientParts::BrowserURLHandlerCreated(
    content::BrowserURLHandler* handler) {
  // rewrite vivaldi: links to long links, and reverse
  if (vivaldi::IsVivaldiRunning()) {
    handler->AddHandlerPair(&vivaldi::HandleVivaldiURLRewrite,
                            content::BrowserURLHandler::null_handler());
  }
}

void VivaldiContentBrowserClientParts::OverrideWebkitPrefs(
    content::RenderViewHost* rvh,
    blink::web_pref::WebPreferences* web_prefs) {
#if !defined(OS_ANDROID)
  if (!vivaldi::IsVivaldiRunning())
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  if (web_contents) {
    // web_contents is nullptr on interstitial pages.
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    PrefService* prefs = profile->GetPrefs();
    web_prefs->tabs_to_links =
        prefs->GetBoolean(vivaldiprefs::kWebpagesTabFocusesLinks);

    // Mouse gestures with the right button and rocker gestures require that
    // we show the context menu on mouse up on all platforms, not only on
    // Windows, to avoid showing it at the start of the gesture.
    if (prefs->GetBoolean(vivaldiprefs::kMouseGesturesEnabled) ||
        prefs->GetBoolean(vivaldiprefs::kMouseGesturesRockerGesturesEnabled)) {
      web_prefs->context_menu_on_mouse_up = true;
    }
    if (vivaldi_runtime_feature::IsEnabled(profile, "double_click_menu") &&
        prefs->GetBoolean(vivaldiprefs::kMouseGesturesDoubleClickMenuEnabled)) {
      web_prefs->vivaldi_show_context_menu_on_double_click = true;
    }
    GURL url = web_contents->GetURL();
    if (url.SchemeIs(content::kChromeUIScheme) &&
        url.host() == vivaldi::kVivaldiGameHost) {
      // Allow sounds without a user gesture first for this specific url, but
      // this method is only called on renderer initialization so this will only
      // work when the game is started in a new tab.
      web_prefs->autoplay_policy =
          blink::mojom::AutoplayPolicy::kNoUserGestureRequired;
    }

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
      if (success) {
        web_prefs->minimum_font_size = min_font_size;
      }
      // No forced dark mode for our UI.
      web_prefs->force_dark_mode_enabled = false;
    }

    // See extension_webkit_preferences::SetPreferences() where some preferences
    // are overridden for platform-apps like Vivaldi.
    extensions::WebViewGuest* guest =
        extensions::WebViewGuest::FromWebContents(web_contents);
    if (guest && guest->IsNavigatingAwayFromVivaldiUI()) {
      web_prefs->databases_enabled = true;
      web_prefs->local_storage_enabled = true;
      web_prefs->sync_xhr_in_documents_enabled = true;
      web_prefs->cookie_enabled = true;
      web_prefs->privileged_webgl_extensions_enabled = false;
    }
#endif //ENABLE_EXTENSIONS
  }
#endif  // !OS_ANDROID
}
