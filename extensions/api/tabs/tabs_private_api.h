// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_TABS_TABS_PRIVATE_API_H_
#define EXTENSIONS_API_TABS_TABS_PRIVATE_API_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/unsafe_shared_memory_region.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_collector.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/session_id.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/zoom/zoom_observer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/drop_data.h"
#include "content/public/renderer/render_frame_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_function.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/drag/drag.mojom.h"
#include "ui/events/keycodes/keyboard_codes.h"

#include "components/content/vivaldi_content_delegates.h"
#include "extensions/schema/tabs_private.h"
#include "renderer/mojo/vivaldi_frame_service.mojom.h"

using JSAccessKeysCallback =
    base::OnceCallback<void(std::vector<::vivaldi::mojom::AccessKeyPtr>)>;

using JSSpatnavRectCallback =
    base::OnceCallback<void(::vivaldi::mojom::SpatnavRectPtr)>;

using JSDetermineTextLanguageCallback =
    base::OnceCallback<void(const std::string&)>;

using JSElementInfoCallback = base::OnceCallback<
    void(const std::string&, const std::string&, bool, const std::string&)>;

class Browser;

namespace extensions {

class TabMutingHandler;

bool IsTabMuted(const content::WebContents* web_contents);
bool IsTabInAWorkspace(const content::WebContents* web_contents);
bool IsTabInAWorkspace(const std::string& viv_extdata);
std::optional<double> GetTabWorkspaceId(const std::string& viv_extdata);
Browser* GetWorkspaceBrowser(const double workspace_id);
int CountTabsInWorkspace(TabStripModel* tab_strip, const double workspace_id);
base::Value::List getLinkRoutes(content::WebContents* contents);
bool SetTabWorkspaceId(content::WebContents* contents, double workspace_id);
bool IsWorkspacesEnabled(content::WebContents* contents);

class TabsPrivateAPI : public BrowserContextKeyedAPI {
  friend class BrowserContextKeyedAPIFactory<TabsPrivateAPI>;
  std::unique_ptr<TabMutingHandler> tabmuting_handler_;
  const raw_ptr<Profile> profile_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "TabsPrivateAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

 public:
  explicit TabsPrivateAPI(content::BrowserContext* context);
  ~TabsPrivateAPI() override;
  TabsPrivateAPI(const TabsPrivateAPI&) = delete;
  TabsPrivateAPI& operator=(const TabsPrivateAPI&) = delete;

  static void Init();

  static TabsPrivateAPI* FromBrowserContext(
      content::BrowserContext* browser_context);

  void UpdateMuting(content::WebContents* active_contents);

  void NotifyTabSelectionChange(content::WebContents* active_contents);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<TabsPrivateAPI>* GetFactoryInstance();

  void NotifyTabChange(content::WebContents* web_contents);
};

// This class holds the Vivaldi-specific render-settings for the lifetime of the
// tab's WebContents.
class VivaldiGuestViewContentObserver
    : public content::WebContentsObserver,
      public zoom::ZoomObserver,
      public content::WebContentsUserData<VivaldiGuestViewContentObserver> {
 public:
  explicit VivaldiGuestViewContentObserver(content::WebContents* web_contents);
  ~VivaldiGuestViewContentObserver() override;
  VivaldiGuestViewContentObserver(const VivaldiGuestViewContentObserver&) =
      delete;
  VivaldiGuestViewContentObserver& operator=(
      const VivaldiGuestViewContentObserver&) = delete;

  static VivaldiGuestViewContentObserver* FromTabId(
      content::BrowserContext* browser_context,
      int tab_id,
      std::string* error);

  // Used for generating JS-event.
  bool show_images() { return show_images_; }
  bool load_from_cache_only() { return load_from_cache_only_; }

  void SetShowImages(bool show_images);
  void SetLoadFromCacheOnly(bool load_from_cache_only);
  void SetMuted(bool mute);

  // Commit setting to the active RenderViewHost.
  void CommitSettings();

 private:
  friend class content::WebContentsUserData<VivaldiGuestViewContentObserver>;

  // content::WebContentsObserver implementation.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void WebContentsDestroyed() override;

  // ZoomObserver implementation.
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;
  void OnZoomControllerDestroyed(
      zoom::ZoomController* zoom_controller) override;

  void SaveZoomLevelToExtData(double zoom_level);
  void SetZoomLevelForTab(double new_level, double old_level);

  void UpdateAllowTabCycleIntoUI();
  void OnPrefsChanged(const std::string& path);

  // We want to communicate changes in some prefs to the renderer right away.
  PrefChangeRegistrar prefs_registrar_;

  // Set to true when we are calling HostZoomMapImpl::SetTemporaryZoomLevel
  // to prevent infinite recursion in observer notification calls
  bool called_host_zoom_ = false;

  // Show images for all pages loaded in this tab. Default is true.
  bool show_images_ = true;

  // Only load the page from cache. Default is false.
  bool load_from_cache_only_ = false;

  // Vivaldi tab zoom level
  double tab_zoom_level_ = -1;

  // The content is muted.
  bool mute_ = false;

  base::WeakPtrFactory<VivaldiGuestViewContentObserver> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

class VivaldiPrivateTabObserver
    : public content::WebContentsObserver,
      public translate::ContentTranslateDriver::TranslationObserver,
      public translate::TranslateDriver::LanguageDetectionObserver,
      public vivaldi_content::TabActivationDelegate,
      public content::WebContentsUserData<VivaldiPrivateTabObserver>,
      public TabResourceUsageCollector::Observer {
 public:
  explicit VivaldiPrivateTabObserver(content::WebContents* web_contents);
  ~VivaldiPrivateTabObserver() override;
  VivaldiPrivateTabObserver(const VivaldiPrivateTabObserver&) = delete;
  VivaldiPrivateTabObserver& operator=(const VivaldiPrivateTabObserver&) =
      delete;

  static VivaldiPrivateTabObserver* FromTabId(
      content::BrowserContext* browser_context,
      int tab_id,
      std::string* error);

  static void BroadcastTabInfo(vivaldi::tabs_private::UpdateTabInfo& info, content::WebContents* web_contents);

  // content::WebContentsObserver implementation.
  void DidChangeThemeColor() override;
  void WebContentsDestroyed() override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void WebContentsDidDetach() override;
  void WebContentsDidAttach() override;
  void BeforeUnloadFired(bool proceed) override;
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void CaptureStarted() override;
  void CaptureFinished() override;
  void MediaPictureInPictureChanged(bool is_picture_in_picture) override;

  // translate::ContentTranslateDriver::Observer implementation
  void OnPageTranslated(const std::string& original_lang,
                        const std::string& translated_lang,
                        translate::TranslateErrors error_type) override;
  void OnIsPageTranslatedChanged(content::WebContents* source) override;

  // translate::TranslateDriver::LanguageDetectionObserver
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;

  // Overridden from vivaldi_content::TabActivationDelegate:
  void ActivateTab(content::WebContents* contents) override;

  void SetContentsMimeType(std::string mimetype) {
    contents_mime_type_ = mimetype;
  }

  std::string contents_mime_type() { return contents_mime_type_; }

  void GetAccessKeys(JSAccessKeysCallback callback);
  void AccessKeysReceived(
      JSAccessKeysCallback callback,
      std::vector<::vivaldi::mojom::AccessKeyPtr> access_keys);

  void AccessKeyAction(std::string);

  void MoveSpatnavRect(::vivaldi::mojom::SpatnavDirection direction,
                       JSSpatnavRectCallback callback);

  void SpatnavRectReceived(JSSpatnavRectCallback callback,
                           ::vivaldi::mojom::SpatnavRectPtr rect);


  void DetermineTextLanguage(const std::string& text,
                             JSDetermineTextLanguageCallback callback);
  void DetermineTextLanguageDone(JSDetermineTextLanguageCallback callback,
                                 const std::string& langCode);

  // If a page is accessing a resource controlled by a permission this will
  // fire.
  void OnPermissionAccessed(ContentSettingsType type,
                            std::string origin,
                            ContentSetting content_setting);

 private:
  friend class content::WebContentsUserData<VivaldiPrivateTabObserver>;

  // TabResourceUsageCollector::Observer:
  void OnTabResourceMetricsRefreshed() override;

  // Mimetype of displayed document.
  std::string contents_mime_type_;

  // Replacement for WEB_CONTENTS_USER_DATA_KEY_DECL() to use an external key
  // from the content.
  static const int& kUserDataKey;

  base::WeakPtrFactory<VivaldiPrivateTabObserver> weak_ptr_factory_{this};
};

class TabsPrivateUpdateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.update", TABSPRIVATE_UPDATE)

  TabsPrivateUpdateFunction() = default;

 protected:
  ~TabsPrivateUpdateFunction() override = default;

 private:
  // BookmarksFunction:
  ResponseAction Run() override;
};

class TabsPrivateGetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.get", TABSPRIVATE_GET)

  TabsPrivateGetFunction() = default;

 protected:
  ~TabsPrivateGetFunction() override = default;

 private:
  // BookmarksFunction:
  ResponseAction Run() override;
};

class TabsPrivateInsertTextFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.insertText", TABSPRIVATE_INSERTTEXT)

  TabsPrivateInsertTextFunction() = default;

 protected:
  ~TabsPrivateInsertTextFunction() override = default;

 private:
  ResponseAction Run() override;
};

class TabsPrivateStartDragFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.startDrag", TABSPRIVATE_STARTDRAG)

  TabsPrivateStartDragFunction();

 protected:
  ~TabsPrivateStartDragFunction() override;

 private:
  ResponseAction Run() override;

  void OnCaptureDone(SessionID::id_type window_id,
                     const url::Origin source_origin,
                     bool success,
                     float device_scale_factor,
                     const SkBitmap& bitmap);

  content::DropData drop_data_;
  blink::mojom::DragEventSourceInfo event_info_;
  gfx::Vector2d image_offset_;
};

class TabsPrivateScrollPageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.scrollPage", TABSPRIVATE_SCROLLPAGE)

  TabsPrivateScrollPageFunction() = default;

 protected:
  ~TabsPrivateScrollPageFunction() override = default;

 private:
  ResponseAction Run() override;
};

class TabsPrivateMoveSpatnavRectFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.moveSpatnavRect",
                             TABSPRIVATE_MOVESPATNAVRECT)

  TabsPrivateMoveSpatnavRectFunction() = default;

 protected:
  ~TabsPrivateMoveSpatnavRectFunction() override = default;

 private:
  void SpatnavRectReceived(::vivaldi::mojom::SpatnavRectPtr rect);

 ResponseAction Run() override;
};

class TabsPrivateActivateSpatnavElementFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.activateSpatnavElement",
                             TABSPRIVATE_ACTIVATESPATNAVELEMENT)

  TabsPrivateActivateSpatnavElementFunction() = default;

 protected:
  ~TabsPrivateActivateSpatnavElementFunction() override = default;

 private:
  ResponseAction Run() override;
};

class TabsPrivateCloseSpatnavOrCurrentOpenMenuFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.closeSpatnavOrCurrentOpenMenu",
                             TABSPRIVATE_CLOSESPATNAVORCURRENTOPENMENU)

  TabsPrivateCloseSpatnavOrCurrentOpenMenuFunction() = default;

 protected:
  ~TabsPrivateCloseSpatnavOrCurrentOpenMenuFunction() override = default;

 private:
  ResponseAction Run() override;
  void CloseSpatnavDone(bool layout_changed, bool current_element_valid);
};

class TabsPrivateHasBeforeUnloadOrUnloadFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.hasBeforeUnloadOrUnload",
                             TABSPRIVATE_HASBEFOREUNLOADORUNLOAD)

  TabsPrivateHasBeforeUnloadOrUnloadFunction() = default;

 protected:
  ~TabsPrivateHasBeforeUnloadOrUnloadFunction() override = default;

 private:
  ResponseAction Run() override;
};

class TabsPrivateTranslatePageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.translatePage",
                             TABSPRIVATE_TRANSLATEPAGE)

  TabsPrivateTranslatePageFunction() = default;

 protected:
  ~TabsPrivateTranslatePageFunction() override = default;

 private:
  ResponseAction Run() override;
};

class TabsPrivateRevertTranslatePageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.revertTranslatePage",
                             TABSPRIVATE_REVERTTRANSLATEPAGE)

  TabsPrivateRevertTranslatePageFunction() = default;

 protected:
  ~TabsPrivateRevertTranslatePageFunction() override = default;

 private:
  ResponseAction Run() override;
};

class TabsPrivateDetermineTextLanguageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.determineTextLanguage",
                             TABSPRIVATE_DETERMINETEXTLANGUAGE)

  TabsPrivateDetermineTextLanguageFunction() = default;

 protected:
  ~TabsPrivateDetermineTextLanguageFunction() override = default;

 private:
  ResponseAction Run() override;
  void DetermineTextLanguageDone(const std::string& langCode);
};

class TabsPrivateLoadViaLifeCycleUnitFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.loadViaLifeCycleUnit",
                             TABSPRIVATE_LOADTHROUGHTABLIFECYCLEUNIT)

  TabsPrivateLoadViaLifeCycleUnitFunction() = default;

 protected:
  ~TabsPrivateLoadViaLifeCycleUnitFunction() override = default;

 private:
  ResponseAction Run() override;
};

class TabsPrivateGetTabPerformanceDataFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.getTabPerformanceData",
                             TABSPRIVATE_GETTABPERFORMANCEDATA)

  TabsPrivateGetTabPerformanceDataFunction() = default;

 protected:
  ~TabsPrivateGetTabPerformanceDataFunction() override = default;

 private:
  ResponseAction Run() override;
};

class TabsPrivateGetSendTabToSelfEntriesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.getSendTabToSelfEntries",
                             TABSPRIVATE_GETSENDTABTOSELFENTRIES)

  TabsPrivateGetSendTabToSelfEntriesFunction() = default;

 protected:
  ~TabsPrivateGetSendTabToSelfEntriesFunction() override = default;

 private:
  ResponseAction Run() override;
};


class TabsPrivateDismissSendTabToSelfEntriesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.dismissSendTabToSelfEntries",
                             TABSPRIVATE_DISMISSSENDTABTOSELFENTRIES)

  TabsPrivateDismissSendTabToSelfEntriesFunction() = default;

 protected:
  ~TabsPrivateDismissSendTabToSelfEntriesFunction() override = default;

 private:
  ResponseAction Run() override;
};

class TabsPrivateExecSendTabToSelfActionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.execSendTabToSelfAction",
                             TABSPRIVATE_EXECSENDTABTOSELFACTION)

  TabsPrivateExecSendTabToSelfActionFunction() = default;

 protected:
  ~TabsPrivateExecSendTabToSelfActionFunction() override = default;

 private:
  ResponseAction Run() override;
};

class TabsPrivateGetSendTabToSelfTargetsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.getSendTabToSelfTargets",
                             TABSPRIVATE_GETSENDTABTOSELFTARGETS)

  TabsPrivateGetSendTabToSelfTargetsFunction() = default;

 protected:
  ~TabsPrivateGetSendTabToSelfTargetsFunction() override = default;

 private:
  ResponseAction Run() override;
};

// Note: SendSend is not a typo, we use "SendTabToSelf" for all related
// functions.
class TabsPrivateSendSendTabToSelfTargetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.sendSendTabToSelfTarget",
                             TABSPRIVATE_SENDSENDTABTOSELFTARGET)

  TabsPrivateSendSendTabToSelfTargetFunction() = default;

 protected:
  ~TabsPrivateSendSendTabToSelfTargetFunction() override = default;

 private:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_TABS_TABS_PRIVATE_API_H_
