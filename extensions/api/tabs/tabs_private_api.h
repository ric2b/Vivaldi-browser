// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_TABS_TABS_PRIVATE_API_H_
#define EXTENSIONS_API_TABS_TABS_PRIVATE_API_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/shared_memory_handle.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/zoom/zoom_observer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_function.h"
#include "renderer/vivaldi_render_messages.h"
#include "ui/events/keycodes/keyboard_codes.h"

typedef base::Callback<void(base::SharedMemoryHandle handle,
                            const gfx::Size image_size,
                            int callback_id,
                            bool success)>
    CaptureTabDoneCallback;

typedef base::OnceCallback<void(
    std::vector<VivaldiViewMsg_AccessKeyDefinition>)>
    AccessKeysCallback;

class TabStripModelObserver;

namespace extensions {


class VivaldiTabsPrivateApiNotification;

class TabsPrivateAPIPrivate;

class TabsPrivateAPI : public BrowserContextKeyedAPI {
 public:
  explicit TabsPrivateAPI(content::BrowserContext* context);
  ~TabsPrivateAPI() override;

  static TabStripModelObserver* GetTabStripModelObserver(
      content::BrowserContext* browser_context);

  static void SendKeyboardShortcutEvent(
      content::BrowserContext* browser_context,
      const content::NativeWebKeyboardEvent& event,
      bool is_auto_repeat);

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<TabsPrivateAPI>* GetFactoryInstance();

  static void SetupWebContents(content::WebContents* web_contents);

  static TabsPrivateAPIPrivate* GetPrivate(content::BrowserContext* context);

 private:
  friend class VivaldiEventHooksImpl;
  friend class BrowserContextKeyedAPIFactory<TabsPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "TabsPrivateAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  std::unique_ptr<TabsPrivateAPIPrivate> priv_;

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

  void BroadcastTabInfo();

  // content::WebContentsObserver implementation.
  void DidChangeThemeColor(SkColor theme_color) override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void WebContentsDestroyed() override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void DocumentAvailableInMainFrame() override;
  void WebContentsDidDetach() override;
  void WebContentsDidAttach() override;

  void SetShowImages(bool show_images);
  void SetLoadFromCacheOnly(bool load_from_cache_only);
  void SetEnablePlugins(bool enable_plugins);
  void SetContentsMimeType(std::string mimetype) {
    contents_mime_type_ = mimetype;
  }
  void UpdateAllowTabCycleIntoUI();
  void UpdateAllowAccessKeys();

  bool show_images() { return show_images_; }
  bool load_from_cache_only() { return load_from_cache_only_; }
  bool enable_plugins() { return enable_plugins_; }
  std::string contents_mime_type() { return contents_mime_type_; }

  // Commit setting to the active RenderViewHost
  void CommitSettings();

  // ZoomObserver implementation.
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;

  void SetZoomLevelForTab(double level);

  void CaptureTab(gfx::Size size,
                  bool full_page,
                  const CaptureTabDoneCallback& callback);

  // Message handlers
  void OnRequestThumbnailForFrameResponse(base::SharedMemoryHandle handle,
                                          const gfx::Size image_size,
                                          int callback_id,
                                          bool success);

  void OnGetAccessKeysForPageResponse(
      std::vector<VivaldiViewMsg_AccessKeyDefinition> access_keys);

  void GetAccessKeys(content::WebContents* tabstrip_contents,
                     AccessKeysCallback callback);

  void AccessKeyAction(content::WebContents* tabstrip_contents, std::string);

  // Returns true if a capture is already underway for this WebContents.
  bool IsCapturing();

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
  double tab_zoom_level_ = 0;

  // Mimetype of displayed document.
  std::string contents_mime_type_;

  // Callback to call when we get an capture response message from the renderer.
  CaptureTabDoneCallback capture_callback_;

  AccessKeysCallback access_keys_callback_;

  // We want to communicate changes in some prefs to the renderer right away.
  PrefChangeRegistrar prefs_registrar_;

  base::WeakPtrFactory<VivaldiPrivateTabObserver> weak_ptr_factory_;

  // Replacement for WEB_CONTENTS_USER_DATA_KEY_DECL() to use an external key
  // from the content.
  static const int& kUserDataKey;

  DISALLOW_COPY_AND_ASSIGN(VivaldiPrivateTabObserver);
};

class TabsPrivateUpdateFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.update", TABSSPRIVATE_UPDATE)

  TabsPrivateUpdateFunction() = default;

 protected:
  ~TabsPrivateUpdateFunction() override = default;

 private:
  // BookmarksFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateUpdateFunction);
};

class TabsPrivateGetFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.get", TABSSPRIVATE_GET)

  TabsPrivateGetFunction() = default;

 protected:
  ~TabsPrivateGetFunction() override = default;

 private:
  // BookmarksFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateGetFunction);
};

class TabsPrivateDiscardFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.discard", TABSSPRIVATE_DISCARD)

  TabsPrivateDiscardFunction() = default;

 protected:
  ~TabsPrivateDiscardFunction() override = default;

 private:
  // BookmarksFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateDiscardFunction);
};

class TabsPrivateInsertTextFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.insertText", TABSSPRIVATE_INSERTTEXT)

  TabsPrivateInsertTextFunction() = default;

 protected:
  ~TabsPrivateInsertTextFunction() override = default;

 private:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateInsertTextFunction);
};

class TabsPrivateStartDragFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.startDrag", TABSSPRIVATE_STARTDRAG)

  TabsPrivateStartDragFunction() = default;

 protected:
  ~TabsPrivateStartDragFunction() override = default;

 private:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateStartDragFunction);
};

class TabsPrivateScrollPageFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.scrollPage", TABSSPRIVATE_SCROLLPAGE)

  TabsPrivateScrollPageFunction() = default;

 protected:
  ~TabsPrivateScrollPageFunction() override = default;

 private:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateScrollPageFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_TABS_TABS_PRIVATE_API_H_
