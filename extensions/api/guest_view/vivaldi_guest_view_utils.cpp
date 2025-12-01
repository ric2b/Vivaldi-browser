// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "extensions/browser/guest_view/web_view/web_view_guest.h"

#include "extensions/api/guest_view/vivaldi_guest_view_utils.h"

// Vivaldi: Detects if given render frame host belongs to a vivaldi tab.
bool IsVivaldiRegularTabFrame(content::RenderFrameHost* frame) {
  auto *guestView = extensions::WebViewGuest::FromRenderFrameHost(frame);
  if (!guestView) return false;
  return guestView->IsVivaldiRegularTab();
}
