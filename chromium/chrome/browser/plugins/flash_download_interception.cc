// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_download_interception.h"

#include "base/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/plugins/plugins_field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/platform/modules/permissions/permission_status.mojom.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/origin.h"

#include "app/vivaldi_apptools.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"

using content::BrowserThread;
using content::NavigationHandle;
using content::NavigationThrottle;

namespace {

const RE2& GetFlashURLCanonicalRegex() {
  static const base::NoDestructor<RE2> re("(?i)get2?\\.adobe\\.com/.*flash.*");
  return *re;
}

const RE2& GetFlashURLSecondaryGoRegex() {
  static const base::NoDestructor<RE2> re(
      "(?i)(www\\.)?(adobe|macromedia)\\.com/go/"
      "((?i).*get[-_]?flash|getfp10android|.*fl(ash)player|.*flashpl|"
      ".*flash_player|flash_completion|flashpm|.*flashdownload|d65_flplayer|"
      "fp_jp|runtimes_fp|[a-z_-]{3,6}h-m-a-?2|chrome|download_player|"
      "gnav_fl|pdcredirect).*");
  return *re;
}

const RE2& GetFlashURLSecondaryDownloadRegex() {
  static const base::NoDestructor<RE2> re(
      "(?i)(www\\.)?(adobe|macromedia)\\.com/shockwave/download/download.cgi");
  return *re;
}

const char kGetFlashURLSecondaryDownloadQuery[] =
    "P1_Prod_Version=ShockwaveFlash";

void PluginLoadResponse(content::WebContents* web_contents,
                        bool allow,
                        const std::string& /* user_input */) {
  if (allow) {
    web_contents->GetController().Reload(content::ReloadType::NORMAL, true);
  }
}

bool InterceptNavigation(
    const GURL& source_url,
    content::WebContents* source,
    const navigation_interception::NavigationParams& params) {
  FlashDownloadInterception::InterceptFlashDownloadNavigation(source,
                                                              source_url);
  return true;
}

}  // namespace

// static
void FlashDownloadInterception::InterceptFlashDownloadNavigation(
    content::WebContents* web_contents,
    const GURL& source_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  ContentSetting flash_setting = PluginUtils::GetFlashPluginContentSetting(
      host_content_settings_map, url::Origin::Create(source_url), source_url,
      nullptr);
  flash_setting = PluginsFieldTrial::EffectiveContentSetting(
      host_content_settings_map, CONTENT_SETTINGS_TYPE_PLUGINS, flash_setting);

  if (flash_setting == CONTENT_SETTING_DETECT_IMPORTANT_CONTENT) {
    // NOTE(andre@vivaldi.com) : The most likely next step is the whole thing is
    // removed.
    if (vivaldi::IsVivaldiRunning()) {
      extensions::WebViewPermissionHelper* permissionhelper =
        extensions::WebViewPermissionHelper::FromWebContents(web_contents);
      if (permissionhelper) {
        base::DictionaryValue request_info;
        request_info.SetString(guest_view::kUrl,
                               url::Origin::Create(source_url).host());
        permissionhelper->RequestPermission(
            WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN, request_info,
            base::Bind(&PluginLoadResponse, web_contents), false);
      }
    } else {
    PermissionManager* manager = PermissionManager::Get(profile);
    manager->RequestPermission(
        CONTENT_SETTINGS_TYPE_PLUGINS, web_contents->GetMainFrame(),
        web_contents->GetLastCommittedURL(), true, base::DoNothing());
    }
  } else if (flash_setting == CONTENT_SETTING_BLOCK) {
    auto* settings = TabSpecificContentSettings::FromWebContents(web_contents);
    if (settings)
      settings->FlashDownloadBlocked();
  }

  // If the content setting has been already changed, do nothing.
}

// static
bool FlashDownloadInterception::ShouldStopFlashDownloadAction(
    HostContentSettingsMap* host_content_settings_map,
    const GURL& source_url,
    const GURL& target_url,
    bool has_user_gesture) {
  if (!PluginUtils::ShouldPreferHtmlOverPlugins(host_content_settings_map))
    return false;

  if (!has_user_gesture)
    return false;

  url::Replacements<char> replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  replacements.ClearUsername();
  replacements.ClearPassword();

  // If the navigation source is already the Flash download page, don't
  // intercept the download. The user may be trying to download Flash.
  std::string source_url_str =
      source_url.ReplaceComponents(replacements).GetContent();

  std::string target_url_str =
      target_url.ReplaceComponents(replacements).GetContent();

  // Early optimization since RE2 is expensive. http://crbug.com/809775
  if (target_url_str.find("adobe.com") == std::string::npos &&
      target_url_str.find("macromedia.com") == std::string::npos)
    return false;

  if (RE2::PartialMatch(source_url_str, GetFlashURLCanonicalRegex()))
    return false;

  if (RE2::FullMatch(target_url_str, GetFlashURLCanonicalRegex()) ||
      RE2::FullMatch(target_url_str, GetFlashURLSecondaryGoRegex()) ||
      (RE2::FullMatch(target_url_str, GetFlashURLSecondaryDownloadRegex()) &&
       target_url.query() == kGetFlashURLSecondaryDownloadQuery)) {
    ContentSetting flash_setting = PluginUtils::GetFlashPluginContentSetting(
        host_content_settings_map, url::Origin::Create(source_url), source_url,
        nullptr);
    flash_setting = PluginsFieldTrial::EffectiveContentSetting(
        host_content_settings_map, CONTENT_SETTINGS_TYPE_PLUGINS,
        flash_setting);

    return flash_setting == CONTENT_SETTING_DETECT_IMPORTANT_CONTENT ||
           flash_setting == CONTENT_SETTING_BLOCK;
  }

  return false;
}

bool CheckIfPluginForMimeIsAvailable(const std::string& mime_type) {
  std::vector<content::WebPluginInfo> plugins;
  content::PluginService::GetInstance()->GetInternalPlugins(&plugins);

  for (size_t i = 0; i < plugins.size(); ++i) {
    const content::WebPluginInfo& plugin = plugins[i];
    const std::vector<content::WebPluginMimeType>& mime_types =
      plugin.mime_types;
    for (size_t j = 0; j < mime_types.size(); ++j) {
      if (mime_types[j].mime_type == mime_type) {
        return true;
      }
    }
  }
  return false;
}

// static
std::unique_ptr<NavigationThrottle>
FlashDownloadInterception::MaybeCreateThrottleFor(NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Vivaldi could be on systems without Flash installed.
  if (vivaldi::IsVivaldiRunning() &&
      !CheckIfPluginForMimeIsAvailable(content::kFlashPluginSwfMimeType)) {
    return nullptr;
  }

  // Browser initiated navigations like the Back button or the context menu
  // should never be intercepted.
  if (!handle->IsRendererInitiated())
    return nullptr;

  // The source URL may be empty, it's a new tab. Intercepting that navigation
  // would lead to a blank new tab, which would be bad.
  GURL source_url = handle->GetWebContents()->GetLastCommittedURL();
  if (source_url.is_empty())
    return nullptr;

  // Always treat main-frame navigations as having a user gesture. We have to do
  // this because the user gesture system can be foiled by popular JavaScript
  // analytics frameworks that capture the click event. crbug.com/678097
  bool has_user_gesture = handle->HasUserGesture() || handle->IsInMainFrame();

  Profile* profile = Profile::FromBrowserContext(
      handle->GetWebContents()->GetBrowserContext());
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  if (!ShouldStopFlashDownloadAction(host_content_settings_map, source_url,
                                     handle->GetURL(), has_user_gesture)) {
    return nullptr;
  }

  return std::make_unique<navigation_interception::InterceptNavigationThrottle>(
      handle, base::Bind(&InterceptNavigation, source_url));
}
