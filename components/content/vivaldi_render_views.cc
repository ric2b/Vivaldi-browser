// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.

#include "content/renderer/render_view_impl.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/web/web_view.h"

using blink::WebPoint;

namespace content {

void RenderViewImpl::OnLoadImageAt(int x, int y) {
  webview()->LoadImageAt(WebPoint(x, y));
}

void RenderViewImpl::ApplyVivaldiSpecificPreferences() {
  if (webview()) {
    webview()->SetImagesEnabled(renderer_preferences_.should_show_images);
    webview()->SetServeResourceFromCacheOnly(
      renderer_preferences_.serve_resources_only_from_cache);
    webview()->SetPluginsEnabled(
      renderer_preferences_.should_enable_plugin_content);
    webview()->SetAllowTabCycleIntoUI(
        renderer_preferences_.allow_tab_cycle_from_webpage_into_ui);
  }
}

}  // namespace content
