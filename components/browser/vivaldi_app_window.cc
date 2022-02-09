// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/browser/app_window/app_window.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/common/url_constants.h"
#include "ui/vivaldi_ui_utils.h"

namespace extensions {

// Only devtools undocked is still using app windows, so ignore it if it
// comes from us.
bool AppWindow::HandleContextMenu(content::RenderFrameHost& render_frame_host,
                                  const content::ContextMenuParams& params) {
  return vivaldi::IsVivaldiRunning() &&
         (params.src_url.SchemeIs(content::kChromeDevToolsScheme) ||
          (params.src_url.host() != vivaldi::kVivaldiAppId));
}

}  // namespace extensions
