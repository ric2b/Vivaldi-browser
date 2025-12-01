// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_BROWSER_COMPONENT_WRAPPER_IMPL_H
#define VIVALDI_BROWSER_COMPONENT_WRAPPER_IMPL_H

// Helper instance to allow access code from non-linked components.
#include "base/scoped_observation.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_action_dispatcher.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_collector.h"
#include "components/content_settings/core/common/content_settings.h"
#include "extensions/vivaldi_browser_component_wrapper.h"

class ExtensionActionDispatcherBridgeImpl
    : public extensions::ExtensionActionDispatcher::Observer {
  base::ObserverList<VivaldiBrowserComponentWrapper::
                         ExtensionActionDispatcherBridge::Observer>::Unchecked
      observers_;

 public:
  void AddObserver(content::BrowserContext* context,
      VivaldiBrowserComponentWrapper::ExtensionActionDispatcherBridge::Observer*
          observer) {
    if (observers_.empty()) { // First call.
      extensions::ExtensionActionDispatcher::Get(context)->AddObserver(this);
    }
    observers_.AddObserver(observer);
  }

  void RemoveObserver(
      content::BrowserContext* context,
      VivaldiBrowserComponentWrapper::ExtensionActionDispatcherBridge::Observer*
          observer) {
    observers_.RemoveObserver(observer);
    if (observers_.empty()) { // Last call.
      extensions::ExtensionActionDispatcher::Get(context)->RemoveObserver(this);
    }
  }

  bool HasObservers() { return !observers_.empty(); }

  // ExtensionActionDispatcher::Observer
  void OnExtensionActionUpdated(
      extensions::ExtensionAction* extension_action,
      content::WebContents* web_contents,
      content::BrowserContext* browser_context) override;
};

class TabResourceUsageCollectorBridgeImpl
  : public TabResourceUsageCollector::Observer {

  base::ObserverList<
      VivaldiBrowserComponentWrapper::TabResourceUsageCollectorBridge::Observer>::
      Unchecked
      observers_;

  public:

  void AddObserver(VivaldiBrowserComponentWrapper::
                        TabResourceUsageCollectorBridge::Observer* observer) {
     if (observers_.empty()) {
       TabResourceUsageCollector::Get()->AddObserver(this);
     }
     observers_.AddObserver(observer);
   }

   void RemoveObserver(
       VivaldiBrowserComponentWrapper::TabResourceUsageCollectorBridge::
           Observer* observer) {
     observers_.RemoveObserver(observer);
     if (observers_.empty()) {
       TabResourceUsageCollector::Get()->RemoveObserver(this);
     }
   }

  //TabResourceUsageCollector::Observer
  void OnTabResourceMetricsRefreshed() override;
};

class ContentSettingChangedBridgeImpl
    : public VivaldiBrowserComponentWrapper::ContentSettingChangedBridge,
      public content_settings::Observer {
  base::ObserverList<ContentSettingChangedBridge::Observer>::Unchecked
      observers_;

  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      observer_{this};

 public:
  ContentSettingChangedBridgeImpl();
  ~ContentSettingChangedBridgeImpl();

  void StartObserving(HostContentSettingsMap* map) { observer_.Observe(map); }

  void StopObserving() { observer_.Reset(); }

  bool HasObservers() { return !observers_.empty(); }

  void AddObserver(ContentSettingChangedBridge::Observer* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(ContentSettingChangedBridge::Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // content_settings::Observer
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type) override;
};

class VivaldiBrowserComponentWrapperImpl
    : public VivaldiBrowserComponentWrapper {
 private:
  // Map with profile and observer(s), one is kept for each profile being added.
  using ProfileContentObserverMap =
      std::map<content::BrowserContext*, ContentSettingChangedBridgeImpl*>;
  ProfileContentObserverMap profile_content_bridge_impl_;

  TabResourceUsageCollectorBridgeImpl tab_resource_usage_bridge_impl_;

  using ProfileExtensionActionDispatcherObserverMap =
      std::map<content::BrowserContext*, ExtensionActionDispatcherBridgeImpl*>;

  ProfileExtensionActionDispatcherObserverMap
      extension_action_dispatcher_bridge_impl_;

 public:
  VivaldiBrowserComponentWrapperImpl();
  ~VivaldiBrowserComponentWrapperImpl();

  void AddContentSettingChangeObserver(
      content::BrowserContext* context,
      ContentSettingChangedBridge::Observer* observer) override;
  void RemoveContentSettingChangeObserver(
      content::BrowserContext* context,
      ContentSettingChangedBridge::Observer* observer) override;

  void AddTabResourceUsageObserver(
    VivaldiBrowserComponentWrapper::TabResourceUsageCollectorBridge::Observer*
    observer) override {
    tab_resource_usage_bridge_impl_.AddObserver(observer);
  }

  void RemoveTabResourceUsageObserver(
    VivaldiBrowserComponentWrapper::TabResourceUsageCollectorBridge::Observer*
    observer) override {
    tab_resource_usage_bridge_impl_.RemoveObserver(observer);
  }

  void AddExtensionActionDispatcherObserver(
      content::BrowserContext* context,
      VivaldiBrowserComponentWrapper::ExtensionActionDispatcherBridge::Observer*
          observer) override;

  void RemoveExtensionActionDispatcherObserver(
      content::BrowserContext* context,
      VivaldiBrowserComponentWrapper::ExtensionActionDispatcherBridge::Observer*
          observer) override;

  ////////////////////////////////////////////

  // Add functions here.
  int BrowserListGetCount() override;
  bool BrowserListHasActive() override;
  void BrowserListInitVivaldiCommandState() override;
  Browser* FindBrowserWithTab(content::WebContents* tab) override;
  Browser* FindBrowserWithWindowId(int window_id) override;
  Browser* FindLastActiveBrowserWithProfile(Profile* profile) override;
  void BrowserDoCloseContents(content::WebContents* tab) override;
  Browser* FindBrowserForEmbedderWebContents(
      content::WebContents* web_contents) override;
  void ShowExtensionErrorDialog(
      Browser* browser,
      extensions::ExternalInstallError* error) override;
  void EnsureTabDialogsCreated(content::WebContents* web_contents) override;
  content::WebContents* BrowserAddNewContents(
      Browser*,
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  content::WebContents* WebViewGuestOpenUrlFromTab(
      content::WebContents* guest_webcontents,
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  bool HandleNonNavigationAboutURL(const GURL& url) override;
  int GetContentSetting(
      content::WebContents* contents,
      const GURL& primary_url,
      const GURL& secondary_url,
      content_settings::mojom::ContentSettingsType content_type) override;
  void SetContentSettingCustomScope(
      content::WebContents* contents,
      bool allow,
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      content_settings::mojom::ContentSettingsType content_type,
      int /* ContentSetting */ setting) override;
  void ProcessMediaAccessRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback,
      const extensions::Extension* extension) override;
  std::vector<Profile*> GetLoadedProfiles() override;
  void CloseAllDevtools() override;
  void AttemptRestart() override;
  void UpdateFromSystemSettings(content::WebContents* contents) override;
  std::string GetDefaultContentSetting(content::BrowserContext* context,
                                       std::string content_setting) override;
  void SetDefaultContentSetting(content::BrowserContext* context,
                                std::string content_setting,
                                std::string default_setting) override;
  void SetContentSettingCustomScope(
      content::BrowserContext* context,
      std::string primary_pattern_string,
      std::string secondary_pattern_string,
      std::string content_type_string,
      std::string content_setting_string) override;

  Browser* GetWorkspaceBrowser(const double workspace_id) override;
  int CountTabsInWorkspace(TabStripModel* tab_strip,
                           const double workspace_id) override;
  VivaldiBrowserWindow* VivaldiBrowserWindowFromId(int id) override;
  VivaldiBrowserWindow* VivaldiBrowserWindowFromBrowser(
      Browser* browser) override;
  VivaldiBrowserWindow* FindWindowForEmbedderWebContents(
      content::WebContents* contents) override;
  int WindowPrivateCreate(
      Profile* profile,
      extensions::vivaldi::window_private::WindowType param_window_type,
      const VivaldiBrowserWindowParams& window_params,
      const gfx::Rect& window_bounds,
      const std::string& window_key,
      const std::string& viv_ext_data,
      const std::string& tab_url,
      base::OnceCallback<void(VivaldiBrowserWindow* window)> callback) override;

  Browser* FindBrowserByWindowId(int32_t window_id) override;
  bool IsOutsideAppWindow(int screen_x, int screen_y) override;
  content::WebContents* FindActiveTabContentsInThisProfile(
      content::BrowserContext* context) override;
  void UpdateMuting(content::WebContents* active_web_contents,
                    vivaldiprefs::TabsAutoMutingValues mute_rule) override;
  int GetTabId(content::WebContents* contents) override;
  int GetWindowIdOfTab(content::WebContents* contents) override;
  void HandleDetachedTabForWebPanel(int tab_id) override;
  content::WebContents* GetWebContentsFromTabStrip(
      content::BrowserContext* browser_context,
      int tab_id,
      std::string* error) override;
  void DoBeforeUnloadFired(content::WebContents* web_contents,
                                   bool proceed,
                                   bool* proceed_to_fire_unload) override;
  void GetTabPerformanceData(content::WebContents* web_contents,
                                     uint64_t& memory_usage,
                                     bool& is_discarded) override;
  void LoadTabContentsIfNecessary(content::WebContents* web_contents) override;
  std::vector<tabs::TabAlert> GetTabAlertStatesForContents(
      content::WebContents* contents) override;
  std::unique_ptr<translate::TranslateUIDelegate> GetTranslateUIDelegate(
      content::WebContents* web_contents,
      std::string& original_language,
      std::string& target_language) override;
  void RevertTranslation(content::WebContents* web_contents) override;
  void ActivateWebContentsInTabStrip(content::WebContents* web_contents) override;
  bool ShowGlobalError(content::BrowserContext* context, int command_id, int window_id) override;
  bool GetGlobalErrors(
      content::BrowserContext* context,
      std::vector<ExtensionInstallError*>& jserrors) override;
  void AddGuestToTabStripModel(content::WebContents* source_content,
                               content::WebContents* guest_content,
                               int windowId,
                               bool activePage,
                               bool inherit_opener,
                               bool is_extension_host) override;

  void WindowRegistryServiceAddWindow(
      content::BrowserContext* browser_context,
      VivaldiBrowserWindow* window,
      const std::string& window_key) override;
  VivaldiBrowserWindow* WindowRegistryServiceGetNamedWindow(
      content::BrowserContext* browser_context,
      const std::string& window_key) override;

  bool ExtensionTabUtilGetTabById(int tab_id,
                                  content::BrowserContext* browser_context,
                                  bool include_incognito,
                                  content::WebContents** contents) override;
  bool ExtensionTabUtilGetTabById(int tab_id,
                                  content::BrowserContext* browser_context,
                                  bool include_incognito,
                                  extensions::WindowController** out_window,
                                  content::WebContents** contents,
                                  int* out_tab_index) override;
  int ExtensionTabUtilGetTabId(const content::WebContents* contents) override;


  bool TopSitesFactoryUpdateNow(
      content::BrowserContext* browser_context) override;
  void TopSitesFactoryAddObserver(content::BrowserContext* browser_context,
                                  history::TopSitesObserver* observer) override;
  void TopSitesFactoryRemoveObserver(
      content::BrowserContext* browser_context,
      history::TopSitesObserver* observer) override;

  bookmarks::BookmarkModel* GetBookmarkModelForBrowserContext(
      content::BrowserContext* browser_context) override;
  const bookmarks::BookmarkNode* GetBookmarkNodeByID(
      bookmarks::BookmarkModel* model, int64_t id) override;


  bool GetControllerFromWindowID(ExtensionFunction* function,
                                 int window_id,
                                 extensions::WindowController** out_controller,
                                 std::string* error) override;
  void LoadViaLifeCycleUnit(content::WebContents* web_contents) override;
  bool SetTabAudioMuted(content::WebContents* contents,
                        bool mute,
                        TabMutedReason reason,
                        const std::string& extension_id) override;

  void NavigationStateChanged(
      VivaldiBrowserWindow* window,
      content::WebContents* web_contents,
      int /*content::InvalidateTypes*/ changed_flags) override;

  // Send to tabs support
  bool GetSendTabToSelfContentHasSupport(
      content::WebContents* web_contents) override;
  bool GetSendTabToSelfModelIsReady(Profile* profile) override;
  bool GetSendTabToSelfReceivedEntries(
      Profile* profile,
      std::vector<SendTabToSelfEntry*>& items) override;
  bool DeleteSendTabToSelfReceivedEntries(
      Profile* profile,
      std::vector<std::string> guids) override;
  bool DismissSendTabToSelfReceivedEntries(
      Profile* profile,
      std::vector<std::string> guids) override;
  bool GetSendTabToSelfTargets(
      Profile* profile,
      std::vector<SendTabToSelfTarget*>& items) override;
  bool SendTabToSelfAddToModel(
      Profile* profile,
      GURL url,
      std::string title,
      std::string guid) override;

  extensions::DevtoolsConnectorItem* ConnectDevToolsWindow(
      content::BrowserContext* browser_context,
      int tab_id,
      content::WebContents* inspected_contents,
      content::WebContentsDelegate* delegate) override;
  content::WebContents*
  DevToolsWindowGetDevtoolsWebContentsForInspectedWebContents(
      content::WebContents* contents) override;

  content::WebContents* DevToolsWindowGetInTabWebContents(
      content::WebContents* inspected_web_contents,
      DevToolsContentsResizingStrategy* out_strategy) override;

  void HandleRegisterHandlerRequest(
      content::WebContents* web_contents,
      custom_handlers::ProtocolHandler* handler) override;
  void SetOrRollbackProtocolHandler(content::WebContents* web_contents,
                                    bool allow) override;
  std::string GetShortcutText(content::BrowserContext* browser_context,
                              extensions::ExtensionAction* action) override;
  bool HasBrowserShortcutPriority(Profile* profile, GURL url) override;

  content::WebContents* GetActiveWebContents(
      content::BrowserContext* browser_context,
      int32_t window_id) override;

  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override;

  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) override;
  void ExitPictureInPicture() override;

  void ShowRepostFormWarningDialog(content::WebContents* source) override;

  void AllowRunningInsecureContent(content::WebContents* web_contents) override;

  void TaskManagerCreateForTabContents(
      content::WebContents* web_contents) override;

  void PageSpecificContentSettingsCreateForTabContents(
      content::WebContents* web_contents) override;

  void CreateWebNavigationTabObserver(
      content::WebContents* web_contents) override;

  extensions::VivaldiPrivateTabObserver*
  VivaldiPrivateTabObserverFromWebContents(content::WebContents* contents) override;

  void OpenExtensionOptionPage(const extensions::Extension* extension,
                                       Browser* browser) override;

  const std::vector<std::unique_ptr<extensions::MenuItem>>*
  GetExtensionMenuItems(content::BrowserContext* context,
                        std::string id) override;

  bool ExecuteCommandMenuItem(content::BrowserContext* browser_context,
                                      std::string extension_id,
                                      int32_t window_id,
                                      std::string menu_id) override;

  //<..>
};

#endif  // once VIVALDI_BROWSER_COMPONENT_WRAPPER_IMPL_H
