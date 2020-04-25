// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "content/browser/browser_plugin/browser_plugin_guest.h"

namespace content {

void BrowserPluginGuest::RenderViewDeleted(RenderViewHost* render_view_host) {
  has_render_view_ = false;
}

}  // namespace content
