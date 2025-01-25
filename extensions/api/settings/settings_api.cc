// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/settings/settings_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_writer.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/schema/settings.h"

namespace extensions {

SettingsSetContentSettingFunction::SettingsSetContentSettingFunction() {}

SettingsSetContentSettingFunction::~SettingsSetContentSettingFunction() {}

ContentSetting ConvertToContentSetting(
    vivaldi::settings::ContentSettingEnum setting) {
  switch (setting) {
    case vivaldi::settings::ContentSettingEnum::kAllow:
      return CONTENT_SETTING_ALLOW;
    case vivaldi::settings::ContentSettingEnum::kBlock:
      return CONTENT_SETTING_BLOCK;
    case vivaldi::settings::ContentSettingEnum::kAsk:
      return CONTENT_SETTING_ASK;
    case vivaldi::settings::ContentSettingEnum::kSessionOnly:
      return CONTENT_SETTING_SESSION_ONLY;
    case vivaldi::settings::ContentSettingEnum::kDetectImportantContent:
      return CONTENT_SETTING_DETECT_IMPORTANT_CONTENT;
    default: {
      NOTREACHED();
      //break;
    }
  }
  //return CONTENT_SETTING_DEFAULT;
}

ContentSettingsType ConvertToContentSettingsType(
    vivaldi::settings::ContentSettingsTypeEnum type) {
  switch (type) {
    case vivaldi::settings::ContentSettingsTypeEnum::kPopups:
      return ContentSettingsType::POPUPS;
    case vivaldi::settings::ContentSettingsTypeEnum::kGeolocation:
      return ContentSettingsType::GEOLOCATION;
    case vivaldi::settings::ContentSettingsTypeEnum::kNotifications:
      return ContentSettingsType::NOTIFICATIONS;
    default: {
      NOTREACHED();
      //break;
    }
  }
  //return ContentSettingsType::DEFAULT;
}

ExtensionFunction::ResponseAction SettingsSetContentSettingFunction::Run() {
  using vivaldi::settings::SetContentSetting::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile);

  const GURL primary_pattern(params->settings_item.primary_pattern);
  const GURL secondary_pattern(params->settings_item.secondary_pattern);

  ContentSettingsType type =
      ConvertToContentSettingsType(params->settings_item.type);
  ContentSetting setting =
      ConvertToContentSetting(params->settings_item.setting);

  content_settings->SetNarrowestContentSetting(
      primary_pattern, secondary_pattern, type, setting);

  return RespondNow(NoArguments());
}

}  // namespace extensions
