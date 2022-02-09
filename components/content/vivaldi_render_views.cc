// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.

#include "content/renderer/render_view_impl.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

void RenderViewImpl::OnLoadImageAt(int x, int y) {
  GetWebView()->LoadImageAt(gfx::Point(x, y));
}

void RenderViewImpl::DidUpdateRendererPreferences() {
  blink::WebView* webview = GetWebView();
  if (webview) {
    auto renderer_preferences = webview->GetRendererPreferences();
    webview->SetImagesEnabled(renderer_preferences.should_show_images);
    webview->SetServeResourceFromCacheOnly(
        renderer_preferences.serve_resources_only_from_cache);
    webview->SetPluginsEnabled(
        renderer_preferences.should_enable_plugin_content);
    webview->SetAllowTabCycleIntoUI(
        renderer_preferences.allow_tab_cycle_from_webpage_into_ui);
    webview->SetAllowAccessKeys(renderer_preferences.allow_access_keys);
  }
}

}  // namespace content
