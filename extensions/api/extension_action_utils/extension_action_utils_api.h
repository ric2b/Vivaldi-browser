// Copyright 2015-2017 Vivaldi Technologies AS. All rights reserved.

#ifndef EXTENSIONS_API_EXTENSION_ACTION_UTILS_EXTENSION_ACTION_UTILS_API_H_
#define EXTENSIONS_API_EXTENSION_ACTION_UTILS_EXTENSION_ACTION_UTILS_API_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
//#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/external_install_error.h"
#include "chrome/browser/extensions/extension_error_ui.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/sessions/core/session_id.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/schema/browser_action_utilities.h"
#include "extensions/vivaldi_browser_component_wrapper.h"

class PrefChangeRegistrar;

namespace extensions {

void FillInfoFromManifest(vivaldi::extension_action_utils::ExtensionInfo* info,
                          const Extension* extension);

typedef std::vector<vivaldi::extension_action_utils::ExtensionInfo>
    ToolbarExtensionInfoList;

class ExtensionActionUtil;
class ExtensionService;

class ExtensionActionUtilFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionActionUtil* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static ExtensionActionUtilFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ExtensionActionUtilFactory>;

  ExtensionActionUtilFactory();
  ~ExtensionActionUtilFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

class ExtensionActionUtil : public KeyedService,
                            public VivaldiBrowserComponentWrapper::
                                ExtensionActionDispatcherBridge::Observer,
                            public extensions::ExtensionRegistryObserver {
  friend struct base::DefaultSingletonTraits<ExtensionActionUtil>;

 public:
  static void SendIconLoaded(content::BrowserContext* browser_context,
                             const std::string& extension_id,
                             const gfx::Image& image);

  explicit ExtensionActionUtil(Profile*);

  ~ExtensionActionUtil() override;

  void FillInfoForTabId(vivaldi::extension_action_utils::ExtensionInfo* info,
                        ExtensionAction* action,
                        int tab_id);

  void GetExtensionsInfo(const ExtensionSet& extensions,
                         extensions::ToolbarExtensionInfoList* extension_list);



 private:
  // KeyedService implementation.
  void Shutdown() override;

  // VivaldiBrowserComponentWrapper::ExtensionActionDispatcherBridge::Observer:
  void OnExtensionActionUpdated(
      ExtensionAction* extension_action,
      content::WebContents* web_contents,
      content::BrowserContext* browser_context) override;

  // Overridden from extensions::ExtensionRegistryObserver:
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  std::unique_ptr<PrefChangeRegistrar> prefs_registrar_;

  const raw_ptr<Profile> profile_;


};

class ExtensionActionUtilsGetToolbarExtensionsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extensionActionUtils.getToolbarExtensions",
                             GETTOOLBAR_EXTENSIONS)
  ExtensionActionUtilsGetToolbarExtensionsFunction() = default;

 private:
  ~ExtensionActionUtilsGetToolbarExtensionsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ExtensionActionUtilsExecuteExtensionActionFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extensionActionUtils.executeExtensionAction",
                             EXTENSIONS_EXECUTEEXTENSIONACTION)
  ExtensionActionUtilsExecuteExtensionActionFunction() = default;

 private:
  ~ExtensionActionUtilsExecuteExtensionActionFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ExtensionActionUtilsRemoveExtensionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extensionActionUtils.removeExtension",
                             EXTENSIONS_REMOVE)
  ExtensionActionUtilsRemoveExtensionFunction() = default;

 private:
  ~ExtensionActionUtilsRemoveExtensionFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ExtensionActionUtilsShowExtensionOptionsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extensionActionUtils.showExtensionOptions",
                             EXTENSIONS_SHOWOPTIONS)
  ExtensionActionUtilsShowExtensionOptionsFunction() = default;

 private:
  ~ExtensionActionUtilsShowExtensionOptionsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ExtensionActionUtilsGetExtensionMenuFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extensionActionUtils.getExtensionMenu",
                             EXTENSIONS_GETEXTENSIONMENU)
  ExtensionActionUtilsGetExtensionMenuFunction() = default;

 private:
  ~ExtensionActionUtilsGetExtensionMenuFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ExtensionActionUtilsExecuteMenuActionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extensionActionUtils.executeMenuAction",
                             EXTENSIONS_EXECUTEMENUACTION)
  ExtensionActionUtilsExecuteMenuActionFunction() = default;

 private:
  ~ExtensionActionUtilsExecuteMenuActionFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ExtensionActionUtilsShowGlobalErrorFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extensionActionUtils.showGlobalError",
                             EXTENSIONS_SHOWGLOBALERROR)
  ExtensionActionUtilsShowGlobalErrorFunction() = default;

 private:
  ~ExtensionActionUtilsShowGlobalErrorFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ExtensionActionUtilsTriggerGlobalErrorsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extensionActionUtils.triggerGlobalErrors",
                             EXTENSIONS_TRIGGERGLOBALERRORS)
  ExtensionActionUtilsTriggerGlobalErrorsFunction() = default;

 private:
  ~ExtensionActionUtilsTriggerGlobalErrorsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

////////


}  // namespace extensions

#endif  // EXTENSIONS_API_EXTENSION_ACTION_UTILS_EXTENSION_ACTION_UTILS_API_H_
