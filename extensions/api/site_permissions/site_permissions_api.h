// Copyright (c) 2015-2025 Vivaldi Technologies AS. All rights reserved.

#ifndef EXTENSIONS_API_PERMISSIONS_API_H_
#define EXTENSIONS_API_PERMISSIONS_API_H_

#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class ContentSettingsObserver;

class SitePermissionsAPI : public BrowserContextKeyedAPI,
                           public content_settings::Observer {
 public:
  explicit SitePermissionsAPI(content::BrowserContext* context);
  ~SitePermissionsAPI() override;
  SitePermissionsAPI(const SitePermissionsAPI&) = delete;
  SitePermissionsAPI& operator=(const SitePermissionsAPI&) = delete;

  static SitePermissionsAPI *FromBrowserContext(
      content::BrowserContext* browser_context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SitePermissionsAPI>* GetFactoryInstance();

  static void SendPermissionChanged(content::BrowserContext* browser_context,
                                    const std::string& origin,
                                    ContentSettingsType type);

 private:
  friend class BrowserContextKeyedAPIFactory<SitePermissionsAPI>;
  friend class ContentSettingsObserver;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "SitePermissionsAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  const raw_ptr<Profile> profile_;
  std::unique_ptr<ContentSettingsObserver> settings_observer_;
};

class SitePermissionsGetAvailablePermissionsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sitePermissions.getAvailablePermissions",
                             SITE_PERMISSIONS_GET_AVAILABLE)
  SitePermissionsGetAvailablePermissionsFunction() = default;
  ResponseAction Run() override;
};

class SitePermissionsGetOverriddenSitesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sitePermissions.getOverriddenSites",
                             SITE_PERMISSIONS_GET_OVERRIDDEN_SITES)
  SitePermissionsGetOverriddenSitesFunction() = default;
  ResponseAction Run() override;
};

class SitePermissionsGetOverridesForSiteFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sitePermissions.getOverridesForSite",
                             SITE_PERMISSIONS_GET_OVERRIDES_FOR_SITE)
  SitePermissionsGetOverridesForSiteFunction() = default;
  ResponseAction Run() override;
};

class SitePermissionsSetSitePermissionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sitePermissions.setSitePermission",
                             SITE_PERMISSIONS_SET_SITE_PERMISSION)
  SitePermissionsSetSitePermissionFunction() = default;
  ResponseAction Run() override;
};

class SitePermissionsResetSitePermissionsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sitePermissions.resetSitePermissions",
                             SITE_PERMISSIONS_RESET_SITE_PERMISSIONS)
  SitePermissionsResetSitePermissionsFunction() = default;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_PERMISSIONS_API_H_
