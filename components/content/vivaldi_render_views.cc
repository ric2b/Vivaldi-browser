// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.

#include "content/renderer/render_view_impl.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

void RenderViewImpl::OnLoadImageAt(int x, int y) {
  GetWebView()->LoadImageAt(gfx::Point(x, y));
}

void RenderViewImpl::ApplyVivaldiSpecificPreferences() {
  if (GetWebView()) {
    GetWebView()->SetImagesEnabled(renderer_preferences_.should_show_images);
    GetWebView()->SetServeResourceFromCacheOnly(
      renderer_preferences_.serve_resources_only_from_cache);
    GetWebView()->SetPluginsEnabled(
      renderer_preferences_.should_enable_plugin_content);
    GetWebView()->SetAllowTabCycleIntoUI(
        renderer_preferences_.allow_tab_cycle_from_webpage_into_ui);
    GetWebView()->SetAllowAccessKeys(renderer_preferences_.allow_access_keys);
  }
}

}  // namespace content
