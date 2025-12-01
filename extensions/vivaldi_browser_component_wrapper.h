// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_BROWSER_COMPONENT_WRAPPER_H
#define VIVALDI_BROWSER_COMPONENT_WRAPPER_H

#include <memory>

#include "base/notreached.h"
#include "vivaldi/prefs/vivaldi_gen_pref_enums.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

// Helper instance to allow access code from non-linked components.

// Only forward declarations are allowed here.
class Browser;
class Profile;
class GURL;
class TabStripModel;
class VivaldiBrowserWindow;
struct VivaldiBrowserWindowParams;
class ExtensionFunction;
struct ExtensionInstallError;
class DevToolsContentsResizingStrategy;
struct SendTabToSelfEntry;
struct SendTabToSelfTarget;

enum class WindowOpenDisposition;
enum class TabAlertState;
enum class TabMutedReason;

namespace blink::mojom {
class WindowFeatures;
}  // namespace blink::mojom

namespace custom_handlers {
class ProtocolHandler;
}  // namespace custom_handlers

namespace extensions {
class DevtoolsConnectorItem;
class ExternalInstallError;
class ExtensionAction;
class Extension;
class MenuItem;
class WindowController;
class VivaldiPrivateTabObserver;
}  // namespace extensions
namespace extensions {
namespace vivaldi {
namespace window_private {
enum class WindowType;
}
}  // namespace vivaldi
}  // namespace extensions

namespace content {
class BrowserContext;
class EyeDropper;
class EyeDropperListener;
struct OpenURLParams;
enum class PictureInPictureResult;
class RenderFrameHost;
class WebContents;
class WebContentsDelegate;
struct MediaStreamRequest;
class MediaStreamUI;
}  // namespace content

// enum class ContentSetting; // NOTE: This is int-casted where used.
class ContentSettingsPattern;
class TabResourceUsageCollector;

namespace content_settings {
namespace mojom {
enum class ContentSettingsType;
}  // namespace mojom
}  // namespace content_settings

namespace blink {
namespace mojom {
enum class MediaStreamRequestResult;
class StreamDevicesSet;
}  // namespace mojom
}  // namespace blink

namespace content {
using MediaResponseCallback = base::OnceCallback<void(
    const blink::mojom::StreamDevicesSet& stream_devices_set,
    blink::mojom::MediaStreamRequestResult result,
    std::unique_ptr<MediaStreamUI> ui)>;
}  // namespace content

namespace gfx {
class Rect;
}

namespace history {
class TopSitesObserver;
}

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace translate {
class TranslateUIDelegate;
}

namespace vivaldi {
class WindowRegistryService;
}

// "\chromium\components\sessions\core\session_id.h"
// class SessionID;
// typedef int32_t id_type;

// A wrapper so that we can call into the browser-component from the extension-
// component when needed.Parameters have to belong in both components, and we
// should try to keep most of our logic in the extension code.
// This is our variant of |ExtensionsBrowserClient|.
// Was introduced because of changes in
// https://issuetracker.google.com/issues/40593486?pli=1 If a browser-side
// method is needed in logic extension-side you can add a method to
// VivaldiBrowserComponentWrapper and call this to jump over the boundary. For
// pattern see :
//    extensions /
//            vivaldi_browser_component_wrapper.h browser /
//            vivaldi_browser_component_wrapper_impl.h
class VivaldiBrowserComponentWrapper {
 public:
  VivaldiBrowserComponentWrapper() = default;

  VivaldiBrowserComponentWrapper(const VivaldiBrowserComponentWrapper&) =
      delete;
  VivaldiBrowserComponentWrapper& operator=(
      const VivaldiBrowserComponentWrapper&) = delete;
  virtual ~VivaldiBrowserComponentWrapper() = default;

  static void CreateImpl();
  static VivaldiBrowserComponentWrapper* GetInstance();
  static void SetInstance(VivaldiBrowserComponentWrapper* wrapper);

  // content_settings::Observer bridge. Fire |OnContentSettingChanged|.
  class ContentSettingChangedBridge {
   public:
    class Observer {
     public:
      virtual void OnContentSettingChanged(
          const ContentSettingsPattern& primary_pattern,
          const ContentSettingsPattern& secondary_pattern,
          int /*ContentSettingsType*/ content_type) {
        NOTREACHED() << "You should not observe without overriding "
                        "OnContentSettingChanged. Fix this.";
      }
    };
  };

  class TabResourceUsageCollectorBridge {
   public:
    class Observer {
     public:
      virtual void OnTabResourceMetricsRefreshed() {
        NOTREACHED() << "You should not observe without overriding "
                        "OnTabResourceMetricsRefreshed. Fix this.";
      }
    };
  };

  class ExtensionActionDispatcherBridge {
   public:
    class Observer {
     public:
      virtual void OnExtensionActionUpdated(
          extensions::ExtensionAction* extension_action,
          content::WebContents* web_contents,
          content::BrowserContext* browser_context) {
        NOTREACHED() << "You should not observe without overriding "
                        "OnExtensionActionUpdated. Fix this.";
      }
    };
  };

  virtual void AddContentSettingChangeObserver(
      content::BrowserContext* context,
      ContentSettingChangedBridge::Observer* observer) = 0;
  virtual void RemoveContentSettingChangeObserver(
      content::BrowserContext* context,
      ContentSettingChangedBridge::Observer* observer) = 0;

  virtual void AddTabResourceUsageObserver(
      TabResourceUsageCollectorBridge::Observer* observer) = 0;
  virtual void RemoveTabResourceUsageObserver(
      TabResourceUsageCollectorBridge::Observer* observer) = 0;

  virtual void AddExtensionActionDispatcherObserver(
      content::BrowserContext* context,
      ExtensionActionDispatcherBridge::Observer* observer) = 0;
  virtual void RemoveExtensionActionDispatcherObserver(
      content::BrowserContext* context,
      ExtensionActionDispatcherBridge::Observer* observer) = 0;

  // Add functions here.
  virtual int BrowserListGetCount() = 0;
  virtual bool BrowserListHasActive() = 0;
  virtual void BrowserListInitVivaldiCommandState() = 0;
  virtual Browser* FindBrowserWithTab(content::WebContents* tab) = 0;
  virtual Browser* FindBrowserWithWindowId(int window_id) = 0;
  virtual Browser* FindLastActiveBrowserWithProfile(Profile* profile) = 0;
  virtual void BrowserDoCloseContents(content::WebContents* tab) = 0;
  virtual Browser* FindBrowserForEmbedderWebContents(
      content::WebContents* web_contents) = 0;
  virtual void ShowExtensionErrorDialog(
      Browser* browser,
      extensions::ExternalInstallError* error) = 0;
  virtual void EnsureTabDialogsCreated(content::WebContents* web_contents) = 0;
  virtual content::WebContents* BrowserAddNewContents(
      Browser*,
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) = 0;
  virtual content::WebContents* WebViewGuestOpenUrlFromTab(
      content::WebContents* guest_webcontents,
      content::WebContents* source,
      const content::OpenURLParams& params) = 0;
  virtual bool HandleNonNavigationAboutURL(const GURL& url) = 0;
  virtual int /* enum ContentSetting */ GetContentSetting(
      content::WebContents* contents,
      const GURL& primary_url,
      const GURL& secondary_url,
      content_settings::mojom::ContentSettingsType content_type) = 0;
  virtual void SetContentSettingCustomScope(
      content::WebContents* contents,
      bool allow,
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      content_settings::mojom::ContentSettingsType content_type,
      int /* enum ContentSetting*/ setting) = 0;
  virtual void ProcessMediaAccessRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback,
      const extensions::Extension* extension) = 0;
  virtual std::vector<Profile*> GetLoadedProfiles() = 0;
  virtual void CloseAllDevtools() = 0;
  virtual void AttemptRestart() = 0;
  virtual void UpdateFromSystemSettings(content::WebContents* contents) = 0;
  virtual std::string GetDefaultContentSetting(content::BrowserContext* context,
                                               std::string content_setting) = 0;
  virtual void SetDefaultContentSetting(content::BrowserContext* context,
                                        std::string content_setting,
                                        std::string default_setting) = 0;
  virtual void SetContentSettingCustomScope(
      content::BrowserContext* context,
      std::string primary_pattern_string,
      std::string secondary_pattern_string,
      std::string content_type_string,
      std::string content_setting_string) = 0;
  virtual Browser* GetWorkspaceBrowser(const double workspace_id) = 0;
  virtual int CountTabsInWorkspace(TabStripModel* tab_strip,
                                   const double workspace_id) = 0;
  virtual VivaldiBrowserWindow* FindWindowForEmbedderWebContents(
      content::WebContents* contents) = 0;
  virtual VivaldiBrowserWindow* VivaldiBrowserWindowFromId(int id) = 0;
  virtual VivaldiBrowserWindow* VivaldiBrowserWindowFromBrowser(
      Browser* browser) = 0;
  virtual int WindowPrivateCreate(
      Profile* profile,
      extensions::vivaldi::window_private::WindowType param_window_type,
      const VivaldiBrowserWindowParams& window_params,
      const gfx::Rect& window_bounds,
      const std::string& window_key,
      const std::string& viv_ext_data,
      const std::string& tab_url,
      base::OnceCallback<void(VivaldiBrowserWindow* window)> callback) = 0;
  virtual Browser* FindBrowserByWindowId(
      int32_t /*SessionId::id_type*/ window_id) = 0;
  virtual bool IsOutsideAppWindow(int x, int y) = 0;
  virtual content::WebContents* FindActiveTabContentsInThisProfile(
      content::BrowserContext* context) = 0;
  virtual void UpdateMuting(content::WebContents* active_web_contents,
                            vivaldiprefs::TabsAutoMutingValues mute_rule) = 0;
  virtual int GetTabId(content::WebContents* contents) = 0;
  virtual int GetWindowIdOfTab(content::WebContents* contents) = 0;
  virtual void HandleDetachedTabForWebPanel(int tab_id) = 0;
  virtual content::WebContents* GetWebContentsFromTabStrip(
      content::BrowserContext* browser_context,
      int tab_id,
      std::string* error) = 0;
  virtual void DoBeforeUnloadFired(content::WebContents* web_contents,
                                   bool proceed,
                                   bool* proceed_to_fire_unload) = 0;
  virtual void GetTabPerformanceData(content::WebContents* web_contents,
                                     uint64_t& memory_usage,
                                     bool& is_discarded) = 0;
  virtual void LoadTabContentsIfNecessary(
      content::WebContents* web_contents) = 0;
  virtual std::vector<TabAlertState> GetTabAlertStatesForContents(
      content::WebContents* contents) = 0;
  virtual std::unique_ptr<translate::TranslateUIDelegate>
  GetTranslateUIDelegate(content::WebContents* web_contents,
                         std::string& original_language,
                         std::string& target_language) = 0;
  virtual void RevertTranslation(content::WebContents* web_contents) = 0;
  virtual void ActivateWebContentsInTabStrip(
      content::WebContents* web_contents) = 0;
  virtual bool ShowGlobalError(content::BrowserContext* context,
                               int command_id,
                               int window_id) = 0;
  virtual bool GetGlobalErrors(
      content::BrowserContext* context,
      std::vector<ExtensionInstallError*>& jserrors) = 0;
  virtual void AddGuestToTabStripModel(content::WebContents* source_content,
                                       content::WebContents* guest_content,
                                       int windowId,
                                       bool activePage,
                                       bool inherit_opener,
                                       bool is_extension_host) = 0;

  virtual void WindowRegistryServiceAddWindow(
      content::BrowserContext* browser_context,
      VivaldiBrowserWindow* window,
      const std::string& window_key) = 0;
  virtual VivaldiBrowserWindow* WindowRegistryServiceGetNamedWindow(
      content::BrowserContext* browser_context,
      const std::string& window_key) = 0;

  virtual bool ExtensionTabUtilGetTabById(
      int tab_id,
      content::BrowserContext* browser_context,
      bool include_incognito,
      content::WebContents** contents) = 0;
  virtual bool ExtensionTabUtilGetTabById(
      int tab_id,
      content::BrowserContext* browser_context,
      bool include_incognito,
      extensions::WindowController** out_window,
      content::WebContents** contents,
      int* out_tab_index) = 0;
  virtual int ExtensionTabUtilGetTabId(
      const content::WebContents* contents) = 0;

  virtual bool TopSitesFactoryUpdateNow(
      content::BrowserContext* browser_context) = 0;
  virtual void TopSitesFactoryAddObserver(
      content::BrowserContext* browser_context,
      history::TopSitesObserver* observer) = 0;
  virtual void TopSitesFactoryRemoveObserver(
      content::BrowserContext* browser_context,
      history::TopSitesObserver* observer) = 0;

  virtual bookmarks::BookmarkModel* GetBookmarkModelForBrowserContext(
      content::BrowserContext* browser_context) = 0;
  virtual const bookmarks::BookmarkNode* GetBookmarkNodeByID(
      bookmarks::BookmarkModel* model,
      int64_t id) = 0;

  virtual bool GetControllerFromWindowID(
      ExtensionFunction* function,
      int window_id,
      extensions::WindowController** out_controller,
      std::string* error) = 0;
  virtual void LoadViaLifeCycleUnit(content::WebContents* contents) = 0;
  virtual bool SetTabAudioMuted(content::WebContents* contents,
                                bool mute,
                                TabMutedReason reason,
                                const std::string& extension_id) = 0;
  virtual void NavigationStateChanged(
      VivaldiBrowserWindow* window,
      content::WebContents* web_contents,
      int /*content::InvalidateTypes*/ changed_flags) = 0;
  // Send to tabs support
  virtual bool GetSendTabToSelfContentHasSupport(
      content::WebContents* web_contents) = 0;
  virtual bool GetSendTabToSelfModelIsReady(Profile* profile) = 0;
  virtual bool GetSendTabToSelfReceivedEntries(
      Profile* profile,
      std::vector<SendTabToSelfEntry*>& items) = 0;
  virtual bool DeleteSendTabToSelfReceivedEntries(
      Profile* profile,
      std::vector<std::string> guids) = 0;
  virtual bool DismissSendTabToSelfReceivedEntries(
      Profile* profile,
      std::vector<std::string> guids) = 0;
  virtual bool GetSendTabToSelfTargets(
      Profile* profile,
      std::vector<SendTabToSelfTarget*>& items) = 0;
  // Add data to model for syncing
  virtual bool SendTabToSelfAddToModel(Profile* profile,
                                       GURL url,
                                       std::string title,
                                       std::string guid) = 0;
  virtual extensions::DevtoolsConnectorItem* ConnectDevToolsWindow(
      content::BrowserContext* browser_context,
      int tab_id,
      content::WebContents* inspected_contents,
      content::WebContentsDelegate* delegate) = 0;
  virtual content::WebContents*
  DevToolsWindowGetDevtoolsWebContentsForInspectedWebContents(
      content::WebContents* contents) = 0;
  virtual content::WebContents* DevToolsWindowGetInTabWebContents(
      content::WebContents* inspected_web_contents,
      DevToolsContentsResizingStrategy* out_strategy) = 0;

  virtual void HandleRegisterHandlerRequest(
      content::WebContents* web_contents,
      custom_handlers::ProtocolHandler* handler) = 0;
  virtual void SetOrRollbackProtocolHandler(content::WebContents* web_contents,
                                            bool allow) = 0;

  virtual extensions::VivaldiPrivateTabObserver*
  VivaldiPrivateTabObserverFromWebContents(content::WebContents* contents) = 0;

  virtual std::string GetShortcutText(content::BrowserContext* browser_context,
                                      extensions::ExtensionAction* action) = 0;
  virtual bool HasBrowserShortcutPriority(Profile* profile, GURL url) = 0;

  virtual content::WebContents* GetActiveWebContents(
      content::BrowserContext* browser_context,
      int32_t window_id) = 0;

  virtual std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) = 0;

  virtual content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) = 0;
  virtual void ExitPictureInPicture() = 0;

  virtual void ShowRepostFormWarningDialog(content::WebContents* source) = 0;

  virtual void AllowRunningInsecureContent(
      content::WebContents* web_contents) = 0;

  virtual void TaskManagerCreateForTabContents(
      content::WebContents* web_contents) = 0;

  virtual void PageSpecificContentSettingsCreateForTabContents(
      content::WebContents* web_contents) = 0;

  virtual void CreateWebNavigationTabObserver(
      content::WebContents* web_contents) = 0;

  virtual void OpenExtensionOptionPage(const extensions::Extension* extension,
                                       Browser* browser) = 0;

  virtual const std::vector<std::unique_ptr<extensions::MenuItem>>*
  GetExtensionMenuItems(content::BrowserContext* context, std::string id) = 0;

  virtual bool ExecuteCommandMenuItem(content::BrowserContext* browser_context,
                                      std::string extension_id,
                                      int32_t window_id,
                                      std::string menu_id) = 0;

  //<..>
};

#endif  // once VIVALDI_BROWSER_COMPONENT_WRAPPER_H
