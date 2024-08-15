// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "extensions/browser/guest_view/extensions_guest_view.h"

#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

void ExtensionsGuestView::ExtensionCanExecuteContentScript(
    const std::string& extension_id,
    ExtensionCanExecuteContentScriptCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool allowScript = false;

  auto* rfh = content::RenderFrameHost::FromID(render_process_id(),
                                               frame_id_.frame_routing_id);
  if (rfh) {
    auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);

    if (web_contents) {
      guest_view::GuestViewBase* guest =
          guest_view::GuestViewBase::FromWebContents(web_contents);
      if (guest) {
        allowScript = guest->owner_host() == extension_id;
      }
    }
  }
  std::move(callback).Run(allowScript);
}

}  // namespace extensions
