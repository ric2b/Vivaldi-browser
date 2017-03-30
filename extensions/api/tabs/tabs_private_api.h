// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_TABS_TABS_PRIVATE_API_H_
#define EXTENSIONS_API_TABS_TABS_PRIVATE_API_H_

#include "base/scoped_observer.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/ui/zoom/zoom_observer.h"

namespace favicon {
  class FaviconDriver;
}

namespace extensions {

class VivaldiTabsPrivateApiNotification;

// Tab contents observer that forward private settings to any new renderer.
// This class holds the Tab-specific settings for the lifetime of the tab's
// WebContents.
class VivaldiPrivateTabObserver
    : public content::WebContentsObserver,
      public ui_zoom::ZoomObserver,
      public content::WebContentsUserData<VivaldiPrivateTabObserver>,
      public favicon::FaviconDriverObserver {
 public:
  ~VivaldiPrivateTabObserver() override;

  // content::WebContentsObserver implementation.
  void DidChangeThemeColor(SkColor theme_color) override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void WebContentsDestroyed() override;

  void SetShowImages(bool show_images);
  void SetLoadFromCacheOnly(bool load_from_cache_only);
  void SetEnablePlugins(bool enable_plugins);

  bool show_images() { return show_images_; }
  bool load_from_cache_only() { return load_from_cache_only_; }
  bool enable_plugins() { return enable_plugins_; }

  // Commit setting to the active RenderViewHost
  void CommitSettings();

  // ZoomObserver implementation.
  void OnZoomChanged(
    const ui_zoom::ZoomController::ZoomChangedEventData& data) override;

  void SetZoomLevelForTab(double level);
  bool IsVivaldiTabZoomEnabled();

  // favicon::FaviconDriverObserver:
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

 private:
  explicit VivaldiPrivateTabObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<VivaldiPrivateTabObserver>;

  static void BroadcastEvent(const std::string& eventname,
                             scoped_ptr<base::ListValue>& args,
                             content::BrowserContext* context);

  void SaveZoomLevelToExtData(double zoom_level);

  // Show images for all pages loaded in this tab. Default is true.
  bool show_images_ = true;

  // Only load the page from cache. Default is false.
  bool load_from_cache_only_ = false;

  // Enable plugins on this tab. Default is true.
  bool enable_plugins_ = true;

  // Vivaldi tab zoom level
  double tab_zoom_level_ = 0;

  ScopedObserver<favicon::FaviconDriver, VivaldiPrivateTabObserver>
      favicon_scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiPrivateTabObserver);
};


class TabsPrivateUpdateFunction: public ChromeAsyncExtensionFunction {
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

class TabsPrivateGetFunction: public ChromeAsyncExtensionFunction {
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

}  // namespace extensions

#endif  // EXTENSIONS_API_TABS_TABS_PRIVATE_API_H_
