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
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/zoom/zoom_observer.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/keycodes/keyboard_codes.h"

typedef std::unordered_map<base::string16, base::string16> TabDragDataCollection;
typedef base::Callback<void(base::SharedMemoryHandle handle,
                            const gfx::Size image_size,
                            int callback_id,
                            bool success)>
    CaptureTabDoneCallback;

namespace extensions {

class VivaldiTabsPrivateApiNotification;

class TabsPrivateEventRouter;

// Interface for forwarding tab drag and drop to extensions.
class TabDragDelegate {
 public:
  virtual void OnDragEnter(const TabDragDataCollection& data) = 0;
  virtual void OnDragOver(const TabDragDataCollection& data) = 0;
  virtual void OnDragLeave(const TabDragDataCollection& data) = 0;
  virtual void OnDrop(const TabDragDataCollection& data) = 0;
  virtual blink::WebDragOperationsMask OnDragEnd(
      int screen_x,
      int screen_y,
      blink::WebDragOperationsMask ops,
      const TabDragDataCollection& data,
      bool cancelled) = 0;
  virtual blink::WebDragOperationsMask OnDragCursorUpdating(
      int screen_x,
      int screen_y,
      blink::WebDragOperationsMask ops) = 0;

 protected:
  virtual ~TabDragDelegate() {}
};

// Class that handles the drag and drop related vivaldi events.
class TabsPrivateEventRouter : public TabDragDelegate {
 public:
  explicit TabsPrivateEventRouter(Profile* profile);
  ~TabsPrivateEventRouter() override;

  // TabDragDelegate interface
  void OnDragEnter(const TabDragDataCollection& data) override;
  void OnDragOver(const TabDragDataCollection& data) override;
  void OnDragLeave(const TabDragDataCollection& data) override;
  void OnDrop(const TabDragDataCollection& data) override;
  blink::WebDragOperationsMask OnDragEnd(int screen_x,
                                         int screen_y,
                                         blink::WebDragOperationsMask ops,
                                         const TabDragDataCollection& data,
                                         bool cancelled) override;
  blink::WebDragOperationsMask OnDragCursorUpdating(
      int screen_x,
      int screen_y,
      blink::WebDragOperationsMask ops) override;

  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     std::unique_ptr<base::ListValue> args);
 private:
  Profile* profile_ = nullptr;
  const std::vector<std::string> tab_drag_data_;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateEventRouter);
};

class TabsPrivateAPI : public BrowserContextKeyedAPI,
                       public TabStripModelObserver,
                       public EventRouter::Observer {
 public:
  explicit TabsPrivateAPI(content::BrowserContext* context);
  ~TabsPrivateAPI() override;

  void SendKeyboardShortcutEvent(
    const content::NativeWebKeyboardEvent& event, bool is_auto_repeat);

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<TabsPrivateAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

  // TabStripModelObserver implementation
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;

  TabDragDelegate* tab_drag_delegate() { return event_router_.get(); }

 private:
  friend class BrowserContextKeyedAPIFactory<TabsPrivateAPI>;

  base::string16 KeyCodeToName(ui::KeyboardCode key_code) const;
  std::string ShortcutText(const content::NativeWebKeyboardEvent& event);

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "TabsPrivateAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  std::unique_ptr<TabsPrivateEventRouter> event_router_;
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

  void SetShowImages(bool show_images);
  void SetLoadFromCacheOnly(bool load_from_cache_only);
  void SetEnablePlugins(bool enable_plugins);
  void SetContentsMimeType(std::string mimetype) {
    contents_mime_type_ = mimetype;
  }
  void UpdateAllowTabCycleIntoUI();

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

  // Returns true if a capture is already underway for this WebContents.
  bool IsCapturing();

  // If a page is accessing a resource controlled by a permission this will
  // fire.
  void OnPermissionAccessed(ContentSettingsType type, std::string origin,
                            ContentSetting content_setting);

  static void BroadcastEvent(const std::string& eventname,
    std::unique_ptr<base::ListValue> args,
    content::BrowserContext* context);

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

  // We want to communicate changes in some prefs to the renderer right away.
  PrefChangeRegistrar prefs_registrar_;

  base::WeakPtrFactory<VivaldiPrivateTabObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiPrivateTabObserver);
};

class TabsPrivateUpdateFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.update", TABSSPRIVATE_UPDATE);

  TabsPrivateUpdateFunction();

 protected:
  ~TabsPrivateUpdateFunction() override;

 private:
  // BookmarksFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateUpdateFunction);
};

class TabsPrivateGetFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.get", TABSSPRIVATE_GET);

  TabsPrivateGetFunction();

 protected:
  ~TabsPrivateGetFunction() override;

 private:
  // BookmarksFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateGetFunction);
};

class TabsPrivateDiscardFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.discard", TABSSPRIVATE_DISCARD);

  TabsPrivateDiscardFunction();

 protected:
  ~TabsPrivateDiscardFunction() override;

 private:
  // BookmarksFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateDiscardFunction);
};

class TabsPrivateInsertTextFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.insertText", TABSSPRIVATE_INSERTTEXT);

  TabsPrivateInsertTextFunction();

 protected:
  ~TabsPrivateInsertTextFunction() override;

 private:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateInsertTextFunction);
};

class TabsPrivateStartDragFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabsPrivate.startDrag", TABSSPRIVATE_STARTDRAG);

  TabsPrivateStartDragFunction();

 protected:
  ~TabsPrivateStartDragFunction() override;

 private:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(TabsPrivateStartDragFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_TABS_TABS_PRIVATE_API_H_
