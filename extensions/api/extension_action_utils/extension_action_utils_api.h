// Copyright 2015-2017 Vivaldi Technologies AS. All rights reserved.

#ifndef EXTENSIONS_API_EXTENSION_ACTION_UTILS_EXTENSION_ACTION_UTILS_API_H_
#define EXTENSIONS_API_EXTENSION_ACTION_UTILS_EXTENSION_ACTION_UTILS_API_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/external_install_error.h"
#include "chrome/browser/extensions/extension_action_dispatcher.h"
#include "chrome/browser/extensions/extension_error_ui.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/sessions/core/session_id.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/schema/browser_action_utilities.h"

class PrefChangeRegistrar;


namespace extensions {

void FillInfoFromManifest(vivaldi::extension_action_utils::ExtensionInfo* info,
                          const Extension* extension);

typedef std::vector<vivaldi::extension_action_utils::ExtensionInfo>
    ToolbarExtensionInfoList;

class ExtensionActionUtil;
class ExtensionService;
class VivaldiExtensionDisabledGlobalError;

using ExtensionToIdMap = base::flat_map<std::string, int>;


// Helper class to map extension id to an unique id used for each error.
class ExtensionToIdProvider {
 public:
  ExtensionToIdProvider();

  ExtensionToIdProvider(
      const ExtensionToIdProvider&) = delete;
  ExtensionToIdProvider& operator=(
      const ExtensionToIdProvider&) = delete;

  ~ExtensionToIdProvider();

  int AddOrGetId(const std::string& extension_id);
  void RemoveExtension(const std::string& extension_id);

  // Used to look up a GlobalError through GetGlobalErrorByMenuItemCommandID.
  // Returns -1 on not-found.
  int GetCommandId(const std::string& extension_id);

 private:
  // Counted unique id.
  int last_used_id_ = 0;

  ExtensionToIdMap extension_ids_;
};

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
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

class ExtensionActionUtil
    : public KeyedService,
      public extensions::ExtensionActionDispatcher::Observer,
      public extensions::ExtensionRegistryObserver,
      public CommandService::Observer {
  friend struct base::DefaultSingletonTraits<ExtensionActionUtil>;

 public:
  static void SendIconLoaded(content::BrowserContext* browser_context,
                             const std::string& extension_id,
                             const gfx::Image& image);

  explicit ExtensionActionUtil(Profile*);

  void FillInfoForTabId(vivaldi::extension_action_utils::ExtensionInfo* info,
                        ExtensionAction* action,
                        int tab_id);

  void GetExtensionsInfo(const ExtensionSet& extensions,
                         extensions::ToolbarExtensionInfoList* extension_list);

  ExtensionToIdProvider& GetExtensionToIdProvider() { return id_provider_; }

  void AddGlobalError(
      std::unique_ptr<VivaldiExtensionDisabledGlobalError> error);

  void RemoveGlobalError(VivaldiExtensionDisabledGlobalError* error);

  raw_ptr<VivaldiExtensionDisabledGlobalError>
  GetGlobalErrorByMenuItemCommandID(int commandid);

  std::vector<std::unique_ptr<VivaldiExtensionDisabledGlobalError>>& errors() {
    return errors_;
  }

 private:
  ~ExtensionActionUtil() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Implementing ExtensionActionAPI::Observer.
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

  // Overridden from CommandService::Observer
  void OnExtensionCommandAdded(const std::string& extension_id,
                               const Command& added_command) override;

  void OnExtensionCommandRemoved(const std::string& extension_id,
                                 const Command& removed_command) override;

  std::unique_ptr<PrefChangeRegistrar> prefs_registrar_;

  const raw_ptr<Profile> profile_;

  // Lookup between extension id and command-id.
  ExtensionToIdProvider id_provider_;

  // Owned global errors.
  std::vector<std::unique_ptr<VivaldiExtensionDisabledGlobalError>> errors_;
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

// Used to show ui in Vivaldi for ExternalInstallBubbleAlert and
// ExtensionDisabledGlobalError.
class VivaldiExtensionDisabledGlobalError
    : public GlobalError,
      public ExtensionUninstallDialog::Delegate,
      public ExtensionRegistryObserver {
 public:
  // ExtensionDisabledGlobalError
  VivaldiExtensionDisabledGlobalError(
      ExtensionService* service,
      const Extension* extension,
      base::WeakPtr<GlobalErrorWithStandardBubble> disabled_upgrade_error);

  // ExtensionDisabledGlobalError
  VivaldiExtensionDisabledGlobalError(content::BrowserContext* context,
                                      base::WeakPtr<ExternalInstallError> error);

  ~VivaldiExtensionDisabledGlobalError() override;

  const Extension* GetExtension();
  const std::string& GetExtensionId() { return extension_id_; }
  const std::string& GetExtensionName() { return extension_name_; }

  void SendGlobalErrorRemoved(
      vivaldi::extension_action_utils::ExtensionInstallError* jserror);

  static void SendGlobalErrorRemoved(
      content::BrowserContext* browser_context,
      vivaldi::extension_action_utils::ExtensionInstallError* jserror);

  // GlobalError implementation.
  int MenuItemCommandID() override;

 private:
  // GlobalError implementation.
  Severity GetSeverity() override;
  bool HasMenuItem() override;
  std::u16string MenuItemLabel() override;
  void ExecuteMenuItem(Browser* browser) override;
  bool HasBubbleView() override;
  bool HasShownBubbleView() override;
  void ShowBubbleView(Browser* browser) override;

  GlobalErrorBubbleViewBase* GetBubbleView() override;
  void SendGlobalErrorAdded(
      vivaldi::extension_action_utils::ExtensionInstallError* jserror);

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;
  void OnShutdown(ExtensionRegistry* registry) override;

  void RemoveGlobalError();

  content::BrowserContext* browser_context_;
  raw_ptr<ExtensionService, DanglingUntriaged> service_;
  scoped_refptr<const Extension> extension_;

  // ExtensionDisabledGlobalError, owned by GlobalErrorService.
  base::WeakPtr<GlobalErrorWithStandardBubble> disabled_upgrade_error_;

  base::WeakPtr<ExternalInstallError> external_install_error_;

  // Copy of the Extension values in case we delete the extension.
  std::string extension_id_;
  std::string extension_name_;
  int command_id_; // Used to lookup error via errorservice or utils.

  std::unique_ptr<ExtensionUninstallDialog> uninstall_dialog_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_API_EXTENSION_ACTION_UTILS_EXTENSION_ACTION_UTILS_API_H_
