// Copyright 2015-2017 Vivaldi Technologies AS. All rights reserved.

#ifndef EXTENSIONS_API_EXTENSION_ACTION_UTILS_EXTENSION_ACTION_UTILS_API_H_
#define EXTENSIONS_API_EXTENSION_ACTION_UTILS_EXTENSION_ACTION_UTILS_API_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/schema/browser_action_utilities.h"

namespace extensions {

class ExtensionActionUtil;

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

// A class observing being an observer on ExtensionActionAPI

class ExtensionActionUtil
    : public KeyedService,
      public extensions::ExtensionActionAPI::Observer,
      public extensions::ExtensionRegistryObserver,
      public TabStripModelObserver,
      public ToolbarActionViewDelegate {
  friend struct base::DefaultSingletonTraits<ExtensionActionUtil>;
  Profile* profile_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  ScopedObserver<ExtensionActionAPI, ExtensionActionAPI::Observer>
      extension_action_api_observer_;

  void OnImageLoaded(const std::string& extension_id, const gfx::Image& image);

  content::WebContents* current_webcontents_ = nullptr;

  // This is only the bundled Chrome cast extension being shown when a cast
  // action is active.
  std::set<std::string> component_extension_actions_;
  std::unique_ptr<ToolbarActionViewController> media_router_component_action_;

 public:
  explicit ExtensionActionUtil(Profile*);

  static void BroadcastEvent(const std::string& eventname,
                             std::unique_ptr<base::ListValue> args,
                             content::BrowserContext* context);

  // Implementing ExtensionActionAPI::Observer.
  void OnExtensionActionUpdated(
      ExtensionAction* extension_action,
      content::WebContents* web_contents,
      content::BrowserContext* browser_context) override;

  // Called when there is a change to the extension action's visibility.
  void OnExtensionActionVisibilityChanged(const std::string& extension_id,
                                          bool is_now_visible) override;

  // Called when the page actions have been refreshed do to a possible change
  // in count or visibility.
  void OnPageActionsUpdated(content::WebContents* web_contents) override;

  // Called when the ExtensionActionAPI is shutting down, giving observers a
  // chance to unregister themselves if there is not a definitive lifecycle.
  void OnExtensionActionAPIShuttingDown() override;

  // Overridden from extensions::ExtensionRegistryObserver:
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionReason reason) override;

  // ToolbarActionViewDelegate:
  content::WebContents* GetCurrentWebContents() const override;

  // Overridden from TabStripModelObserver:
  void OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) override;

  // Updates the view to reflect current state.
  void UpdateState() override;

  // Returns true if a context menu is running.
  bool IsMenuRunning() const override;

  // Encodes the passed bitmap as a PNG represented as a dataurl.
  static std::string* EncodeBitmapToPng(const SkBitmap* bitmap);

  void set_current_webcontents(content::WebContents* contents) {
    current_webcontents_ = contents;
  }

  std::set<std::string> component_extension_actions() {
    return component_extension_actions_;
  }

  ToolbarActionViewController* media_router_component_action() {
    return media_router_component_action_.get();
  }
 private:
  ~ExtensionActionUtil() override;

  base::WeakPtrFactory<ExtensionActionUtil> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionUtil);
};

class ExtensionActionUtilsGetToolbarExtensionsFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extensionActionUtils.getToolbarExtensions",
                             GETTOOLBAR_EXTENSIONS)
  ExtensionActionUtilsGetToolbarExtensionsFunction();

 private:
  ~ExtensionActionUtilsGetToolbarExtensionsFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;
  DISALLOW_COPY_AND_ASSIGN(ExtensionActionUtilsGetToolbarExtensionsFunction);
};

class ExtensionActionUtilsExecuteExtensionActionFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extensionActionUtils.executeExtensionAction",
                             EXECUTE_EXTENSIONACTION)
  ExtensionActionUtilsExecuteExtensionActionFunction();

 private:
  ~ExtensionActionUtilsExecuteExtensionActionFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionUtilsExecuteExtensionActionFunction);
};

class ExtensionActionUtilsToggleBrowserActionVisibilityFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "extensionActionUtils.toggleBrowserActionVisibility",
      TOGGLE_BROWSERACTIONVISIBILITY)
  ExtensionActionUtilsToggleBrowserActionVisibilityFunction();

 private:
  ~ExtensionActionUtilsToggleBrowserActionVisibilityFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(
      ExtensionActionUtilsToggleBrowserActionVisibilityFunction);
};

class ExtensionActionUtilsRemoveExtensionFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extensionActionUtils.removeExtension",
                             EXTENSIONS_REMOVE)
  ExtensionActionUtilsRemoveExtensionFunction();

 private:
  ~ExtensionActionUtilsRemoveExtensionFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionUtilsRemoveExtensionFunction);
};

class ExtensionActionUtilsShowExtensionOptionsFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extensionActionUtils.showExtensionOptions",
                             EXTENSIONS_SHOWOPTIONS)
  ExtensionActionUtilsShowExtensionOptionsFunction() = default;

 private:
  ~ExtensionActionUtilsShowExtensionOptionsFunction() override = default;

  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionUtilsShowExtensionOptionsFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_EXTENSION_ACTION_UTILS_EXTENSION_ACTION_UTILS_API_H_
