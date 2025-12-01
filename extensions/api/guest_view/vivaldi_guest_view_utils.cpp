// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "extensions/browser/guest_view/web_view/web_view_guest.h"

#include "extensions/api/guest_view/vivaldi_guest_view_utils.h"

#include "app/vivaldi_apptools.h"

// Vivaldi: Detects if given render frame host belongs to a vivaldi tab.
bool IsVivaldiRegularTabFrame(content::RenderFrameHost* frame) {
  auto *guestView = extensions::WebViewGuest::FromRenderFrameHost(frame);
  if (!guestView) return false;
  return guestView->IsVivaldiRegularTab();
}

bool IsVivaldiEditorFrame(content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents = content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return false;

  extensions::WebViewGuest* webview_guest = extensions::WebViewGuest::FromWebContents(web_contents);
  if (!webview_guest || !webview_guest->owner_rfh())
    return false;

  // Check embedder extension ID via owner URL scheme+host.
  const GURL& owner_site_url = webview_guest->GetOwnerLastCommittedURL();
  if (!owner_site_url.SchemeIs(extensions::kExtensionScheme))
    return false;

  std::string embedder_extension_id = owner_site_url.host();
  if (!vivaldi::IsVivaldiApp(embedder_extension_id))
    return false;

  return webview_guest->IsVivaldiEditorView();
}
