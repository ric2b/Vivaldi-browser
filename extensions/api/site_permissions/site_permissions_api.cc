// Copyright (c) 2015-2025 Vivaldi Technologies AS. All rights reserved.
#include "extensions/api/site_permissions/site_permissions_api.h"

#include "base/scoped_observation.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_request.h"
#include "extensions/browser/api/content_settings/content_settings_helpers.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/site_permissions.h"
#include "extensions/tools/vivaldi_tools.h"

namespace extensions {

constexpr std::array<ContentSetting, 4> kAllContentSettings = {
    CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK,
    CONTENT_SETTING_SESSION_ONLY,
    // NOTE: CONTENT_SETTING_DETECT_IMPORTANT_CONTENT
};

vivaldi::site_permissions::PermissionSetting toPermissionSetting(
    ContentSetting setting) {
  using vivaldi::site_permissions::PermissionSetting;
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return PermissionSetting::kAllow;
    case CONTENT_SETTING_BLOCK:
      return PermissionSetting::kBlock;
    case CONTENT_SETTING_ASK:
      return PermissionSetting::kAsk;
    case CONTENT_SETTING_SESSION_ONLY:
      return PermissionSetting::kSessionOnly;
    case CONTENT_SETTING_DEFAULT:
      break;
    default:
      break;
  }

  return PermissionSetting::kNone;
}

std::optional<ContentSetting> fromPermissionSetting(
    vivaldi::site_permissions::PermissionSetting setting) {
  switch (setting) {
    case vivaldi::site_permissions::PermissionSetting::kAllow:
      return CONTENT_SETTING_ALLOW;
    case vivaldi::site_permissions::PermissionSetting::kBlock:
      return CONTENT_SETTING_BLOCK;
    case vivaldi::site_permissions::PermissionSetting::kAsk:
      return CONTENT_SETTING_ASK;
    case vivaldi::site_permissions::PermissionSetting::kSessionOnly:
      return CONTENT_SETTING_SESSION_ONLY;
    case vivaldi::site_permissions::PermissionSetting::kNone:
      return std::nullopt;
  }
  return std::nullopt;
}

class ContentSettingsObserver : public content_settings::Observer {
 public:
  ContentSettingsObserver(SitePermissionsAPI* owner)
      : owner_{owner},
        host_content_settings_map_{
            HostContentSettingsMapFactory::GetForProfile(owner->profile_)} {
    observer_.Observe(host_content_settings_map_);
  }

  // Per profile observation of content settings.
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      observer_{this};

  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type) override {
    DCHECK(owner_);
    SitePermissionsAPI::SendPermissionChanged(
        static_cast<content::BrowserContext*>(owner_->profile_.get()),
        primary_pattern.ToString(), content_type);
  }

  SitePermissionsAPI* owner_;
  HostContentSettingsMap* host_content_settings_map_;
};

SitePermissionsAPI::SitePermissionsAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  settings_observer_.reset(new ContentSettingsObserver(this));
}

SitePermissionsAPI::~SitePermissionsAPI() {}

SitePermissionsAPI* SitePermissionsAPI::FromBrowserContext(
    content::BrowserContext* browser_context) {
  SitePermissionsAPI* api = GetFactoryInstance()->Get(browser_context);
  DCHECK(api);
  return api;
}

// static
BrowserContextKeyedAPIFactory<SitePermissionsAPI>*
SitePermissionsAPI::GetFactoryInstance() {
  static base::NoDestructor<BrowserContextKeyedAPIFactory<SitePermissionsAPI>>
      instance;
  return instance.get();
}

// static
void SitePermissionsAPI::SendPermissionChanged(
    content::BrowserContext* browser_context,
    const std::string& origin,
    ContentSettingsType type) {
  vivaldi::site_permissions::PermissionChangeEvent event;

  if (!content_settings::mojom::IsKnownEnumValue(type)) return;

  // Don't propagate DEFAULT it will crash...
  if (type == ContentSettingsType::DEFAULT) return;

  event.origin = origin;
  event.permission = content_settings_helpers::ContentSettingsTypeToString(type);

  ::vivaldi::BroadcastEvent(vivaldi::site_permissions::OnPermissionChanged::kEventName,
                            vivaldi::site_permissions::OnPermissionChanged::Create(event),
                            browser_context);
}

ExtensionFunction::ResponseAction
SitePermissionsGetAvailablePermissionsFunction::Run() {
  using vivaldi::site_permissions::PermissionOptions;
  using vivaldi::site_permissions::PermissionSetting;
  namespace Results =
      vivaldi::site_permissions::GetAvailablePermissions::Results;

  std::vector<PermissionOptions> permissions;

  content_settings::ContentSettingsRegistry* registry =
      content_settings::ContentSettingsRegistry::GetInstance();

  for (const content_settings::ContentSettingsInfo* info : *registry) {
    auto website_info = info->website_settings_info();

    PermissionOptions permission_entry;

    permission_entry.permission =
        content_settings_helpers::ContentSettingsTypeToString(
            website_info->type());

    // Iterate all possible options (hardcoded)
    for (ContentSetting setting : kAllContentSettings) {
      if (!info->IsSettingValid(setting)) {
        continue;
      }
      auto jsetting = toPermissionSetting(setting);
      if (jsetting != PermissionSetting::kNone) {
        permission_entry.options.push_back(jsetting);
      }
    }

    auto jsetting = toPermissionSetting(info->GetInitialDefaultSetting());
    if (jsetting != PermissionSetting::kNone) {
      permission_entry.default_ = jsetting;
    }

    permissions.push_back(std::move(permission_entry));
  }

  return RespondNow(ArgumentList(Results::Create(std::move(permissions))));
}

ExtensionFunction::ResponseAction
SitePermissionsGetOverriddenSitesFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!profile)
    return RespondNow(Error("Could not get profile"));

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  if (!settings_map)
    return RespondNow(Error("Could not access content settings"));

  base::flat_set<std::string> origin_set;

  const content_settings::ContentSettingsRegistry* registry =
      content_settings::ContentSettingsRegistry::GetInstance();

  for (const auto* info : *registry) {
    ContentSettingsType type = info->website_settings_info()->type();

    auto entries = settings_map->GetSettingsForOneType(type);

    for (const auto& entry : entries) {
      ContentSetting setting =
          content_settings::ValueToContentSetting(entry.setting_value);
      if (setting == CONTENT_SETTING_DEFAULT)
        continue;

      std::string origin = entry.primary_pattern.ToString();
      if (!origin.empty())
        origin_set.insert(origin);
    }
  }

  std::vector<std::string> origins(origin_set.begin(), origin_set.end());
  return RespondNow(ArgumentList(
      vivaldi::site_permissions::GetOverriddenSites::Results::Create(origins)));
}

ExtensionFunction::ResponseAction
SitePermissionsGetOverridesForSiteFunction::Run() {
  using vivaldi::site_permissions::GetOverridesForSite::Params;
  namespace Results = vivaldi::site_permissions::GetOverridesForSite::Results;
  using vivaldi::site_permissions::Permission;

  std::optional<Params> params = Params::Create(args());
  if (!params)
    return RespondNow(Error("Invalid parameters."));

  const std::string& origin_str = params->origin;
  GURL origin_url(origin_str);
  if (!origin_url.is_valid())
    return RespondNow(Error("Invalid origin URL."));

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!profile)
    return RespondNow(Error("Could not get profile."));

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  if (!settings_map)
    return RespondNow(Error("Could not access content settings."));

  const auto* registry =
      content_settings::ContentSettingsRegistry::GetInstance();
  std::vector<Permission> result;

  for (const auto* info : *registry) {
    ContentSettingsType type = info->website_settings_info()->type();

    const base::Value setting_value = settings_map->GetWebsiteSetting(
        origin_url, origin_url, type, nullptr);

    if (setting_value == base::Value())
      continue;

    ContentSetting setting =
        content_settings::ValueToContentSetting(setting_value);
    if (setting == CONTENT_SETTING_DEFAULT)
      continue;

    if (setting == info->GetInitialDefaultSetting())
      continue;

    Permission permission;
    permission.origin = origin_str;
    permission.permission =
        content_settings_helpers::ContentSettingsTypeToString(
            info->website_settings_info()->type());
    permission.setting = toPermissionSetting(setting);

    result.push_back(std::move(permission));
  }

  return RespondNow(ArgumentList(Results::Create(result)));
}

ExtensionFunction::ResponseAction
SitePermissionsSetSitePermissionFunction::Run() {
  using vivaldi::site_permissions::SetSitePermission::Params;
  namespace Results = vivaldi::site_permissions::SetSitePermission::Results;

  std::optional<Params> params = Params::Create(args());
  if (!params) {
    return RespondNow(Error("Invalid parameters."));
  }

  // Sanitize the params
  GURL origin_url(params->origin);
  if (!origin_url.is_valid()) {
    return RespondNow(Error("Invalid origin URL."));
  }

  // Convert permission string to content setting type.
  std::optional<ContentSettingsType> type_opt =
      content_settings_helpers::StringToContentSettingsType(params->permission);
  if (!type_opt) {
    return RespondNow(Error("Unknown permission type: " + params->permission));
  }
  std::optional<ContentSetting> setting_opt =
      fromPermissionSetting(params->setting);
  if (!setting_opt)
    return RespondNow(Error("Invalid setting value"));

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!profile) {
    return RespondNow(Error("Could not get profile."));
  }

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  if (!settings_map) {
    return RespondNow(Error("Could not access content settings."));
  }


  content_settings::ContentSettingsRegistry* registry =
      content_settings::ContentSettingsRegistry::GetInstance();

  auto info = registry->Get(*type_opt);

  if (!info) {
    return RespondNow(Error("Could not retrieve the content setting info."));
  }

  if (!info->IsSettingValid(*setting_opt)) {
    return RespondNow(Error("Invalid setting type for given permission type."));
  }

  settings_map->SetContentSettingDefaultScope(origin_url, GURL(), *type_opt,
                                              *setting_opt);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SitePermissionsResetSitePermissionsFunction::Run() {
  using vivaldi::site_permissions::ResetSitePermissions::Params;
  namespace Results = vivaldi::site_permissions::GetOverridesForSite::Results;
  using vivaldi::site_permissions::Permission;

  std::optional<Params> params = Params::Create(args());
  if (!params)
    return RespondNow(Error("Invalid parameters."));

  const std::string& origin_str = params->origin;
  GURL origin_url(origin_str);
  if (!origin_url.is_valid())
    return RespondNow(Error("Invalid origin URL."));

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!profile) {
    return RespondNow(Error("Could not get profile."));
  }

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  if (!settings_map) {
    return RespondNow(Error("Could not access content settings."));
  }
  const auto* registry =
      content_settings::ContentSettingsRegistry::GetInstance();

  for (const auto* info : *registry) {
    ContentSettingsType type = info->website_settings_info()->type();

    const base::Value setting_value = settings_map->GetWebsiteSetting(
        origin_url, origin_url, type, nullptr);

    if (setting_value == base::Value())
      continue;

    ContentSetting setting =
        content_settings::ValueToContentSetting(setting_value);
    if (setting == CONTENT_SETTING_DEFAULT)
      continue;

    if (setting == info->GetInitialDefaultSetting())
      continue;

    settings_map->SetContentSettingDefaultScope(origin_url, GURL(), type,
                                                CONTENT_SETTING_DEFAULT);
  }

  return RespondNow(NoArguments());
}

}  // namespace extensions
