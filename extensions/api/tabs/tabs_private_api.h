// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_TABS_TABS_PRIVATE_API_H_
#define EXTENSIONS_API_TABS_TABS_PRIVATE_API_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/zoom/zoom_observer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/drop_data.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_function.h"
#include "renderer/vivaldi_render_messages.h"
#include "third_party/blink/public/mojom/page/drag.mojom.h"
#include "ui/events/keycodes/keyboard_codes.h"

typedef base::OnceCallback<void(
    std::vector<VivaldiViewMsg_AccessKeyDefinition>)>
    AccessKeysCallback;

typedef base::OnceCallback<void(
    std::vector<VivaldiViewMsg_NavigationRect>)>
    GetSpatialNavigationRectsCallback;

typedef base::OnceCallback<void(int, int)>
    GetScrollPositionCallback;

class TabStripModelObserver;

namespace extensions {

class VivaldiTabsPrivateApiNotification;

class TabsPrivateAPI : public BrowserContextKeyedAPI,
                       public TabStripModelObserver,
                       public javascript_dialogs::AppModalDialogObserver {
 public:
  explicit TabsPrivateAPI(content::BrowserContext* context);
  ~TabsPrivateAPI() override;

  static TabsPrivateAPI* FromBrowserContext(
      content::BrowserContext* browser_context);

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<TabsPrivateAPI>* GetFactoryInstance();

 private:
  friend class VivaldiEventHooksImpl;
  friend class BrowserContextKeyedAPIFactory<TabsPrivateAPI>;

  // TabStripModelObserver implementation
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;

  // javascript_dialogs::AppModalDialogObserver implementation
  void Notify(javascript_dialogs::AppModalDialogController* dialog) override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "TabsPrivateAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateAPI);
};

// Tab contents observer that forward private settings to any new renderer.
// This class holds the Tab-specific settings for the lifetime of the tab's
// WebContents.
class VivaldiPrivateTabObserver
    : public content::WebContentsObserver,
      public zoom::ZoomObserver,
      public content::WebContentsUserData<VivaldiPrivateTabObserver> {
 public:
  explicit VivaldiPrivateTabObserver(content::WebContents* web_contents);
  ~VivaldiPrivateTabObserver() override;

  static VivaldiPrivateTabObserver* FromTabId(
      content::BrowserContext* browser_context,
      int tab_id,
      std::string* error);

  void BroadcastTabInfo();

  // content::WebContentsObserver implementation.
  void DidChangeThemeColor() override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void WebContentsDestroyed() override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                             const GURL& validated_url) override;
  void WebContentsDidDetach() override;
  void WebContentsDidAttach() override;
  void BeforeUnloadFired(bool proceed,
                         const base::TimeTicks& proceed_time) override;

  void SetShowImages(bool show_images);
  void SetLoadFromCacheOnly(bool load_from_cache_only);
  void SetEnablePlugins(bool enable_plugins);
  void SetContentsMimeType(std::string mimetype) {
    contents_mime_type_ = mimetype;
  }

  void SetMuted(bool mute);

  void UpdateAllowTabCycleIntoUI();
  void UpdateAllowAccessKeys();

  bool show_images() { return show_images_; }
  bool load_from_cache_only() { return load_from_cache_only_; }
  bool enable_plugins() { return enable_plugins_; }
  std::string contents_mime_type() { return contents_mime_type_; }
  bool mute() { return mute_; }

  // Commit setting to the active RenderViewHost
  void CommitSettings();

  // ZoomObserver implementation.
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;

  void SetZoomLevelForTab(double new_level, double old_level);

  // Message handlers
  void OnGetAccessKeysForPageResponse(
      std::vector<VivaldiViewMsg_AccessKeyDefinition> access_keys);

  void GetAccessKeys(AccessKeysCallback callback);

  void AccessKeyAction(std::string);

  void GetSpatialNavigationRects(GetSpatialNavigationRectsCallback callback);
  void OnGetSpatialNavigationRectsResponse(
      std::vector<VivaldiViewMsg_NavigationRect> rect);

  void GetScrollPosition(GetScrollPositionCallback callback);
  void OnGetScrollPositionResponse(int x, int y);

  // If a page is accessing a resource controlled by a permission this will
  // fire.
  void OnPermissionAccessed(ContentSettingsType type, std::string origin,
                            ContentSetting content_setting);

private:
  friend class content::WebContentsUserData<VivaldiPrivateTabObserver>;

  void SaveZoomLevelToExtData(double zoom_level);

  void OnPrefsChanged(const std::string& path);

  // Show images for all pages loaded in this tab. Default is true.
  bool show_images_ = true;

  // Only load the page from cache. Default is false.
  bool load_from_cache_only_ = false;

  // Enable plugins on this tab. Default is true.
  bool enable_plugins_ = true;

  // Vivaldi tab zoom level
  double tab_zoom_level_ = -1;

  // Mimetype of displayed document.
  std::string contents_mime_type_;

  // The tab is muted.
  bool mute_ = false;

  AccessKeysCallback access_keys_callback_;

  GetSpatialNavigationRectsCallback spatnav_callback_;

  GetScrollPositionCallback scroll_position_callback_;

  // We want to communicate changes in some prefs to the renderer right away.
  PrefChangeRegistrar prefs_registrar_;

  base::WeakPtrFactory<VivaldiPrivateTabObserver> weak_ptr_factory_;

  // Replacement for WEB_CONTENTS_USER_DATA_KEY_DECL() to use an external key
  // from the content.
  static const int& kUserDataKey;

  DISALLOW_COPY_AND_ASSIGN(VivaldiPrivateTabObserver);
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

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateUpdateFunction);
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

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateGetFunction);
};

class TabsPrivateDiscardFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.discard", TABSPRIVATE_DISCARD)

  TabsPrivateDiscardFunction() = default;

 protected:
  ~TabsPrivateDiscardFunction() override = default;

 private:
  // BookmarksFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateDiscardFunction);
};

class TabsPrivateInsertTextFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.insertText", TABSPRIVATE_INSERTTEXT)

  TabsPrivateInsertTextFunction() = default;

 protected:
  ~TabsPrivateInsertTextFunction() override = default;

 private:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateInsertTextFunction);
};

class TabsPrivateStartDragFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.startDrag", TABSPRIVATE_STARTDRAG)

  TabsPrivateStartDragFunction();

 protected:
  ~TabsPrivateStartDragFunction() override;

 private:
  ResponseAction Run() override;

  void OnCaptureDone(bool success,
                     float device_scale_factor,
                     const SkBitmap& bitmap);

  content::DropData drop_data_;
  blink::mojom::DragEventSourceInfo event_info_;
  gfx::Vector2d image_offset_;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateStartDragFunction);
};

class TabsPrivateScrollPageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.scrollPage", TABSPRIVATE_SCROLLPAGE)

  TabsPrivateScrollPageFunction() = default;

 protected:
  ~TabsPrivateScrollPageFunction() override = default;

 private:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateScrollPageFunction);
};

class TabsPrivateGetSpatialNavigationRectsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.getSpatialNavigationRects",
                             TABSPRIVATE_GETSPATIALNAVIGATIONRECTS)

  TabsPrivateGetSpatialNavigationRectsFunction() = default;

 protected:
  ~TabsPrivateGetSpatialNavigationRectsFunction() override = default;

 private:
  void GetSpatialNavigationRectsResponse(
      std::vector<VivaldiViewMsg_NavigationRect> rects);

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateGetSpatialNavigationRectsFunction);
};

class TabsPrivateGetScrollPositionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.getScrollPosition",
                             TABSPRIVATE_GETSCROLLPOSITION)

 TabsPrivateGetScrollPositionFunction() = default;

 protected:
  ~TabsPrivateGetScrollPositionFunction() override = default;

 private:
  void GetScrollPositionResponse(int x, int y);

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateGetScrollPositionFunction);
};

class TabsPrivateActivateElementFromPointFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.activateElementFromPoint",
                             TABSPRIVATE_ACTIVATEELEMENTFROMPOINT)

  TabsPrivateActivateElementFromPointFunction() = default;

 protected:
  ~TabsPrivateActivateElementFromPointFunction() override = default;

 private:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateActivateElementFromPointFunction);
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

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateHasBeforeUnloadOrUnloadFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_TABS_TABS_PRIVATE_API_H_
