// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "renderer/mojo/vivaldi_encrypted_media_access.mojom.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"

namespace vivaldi {

void NotifyEncryptedMediaAccessRequest(const blink::WebString& key_system,
                                       blink::LocalDOMWindow* window) {
  // This is in render process, we use mojom to send the notification.
  auto* frame = window->GetFrame();

  auto* interfaces = frame->Client()->GetRemoteNavigationAssociatedInterfaces();
  if (!interfaces)
    return;

  mojo::AssociatedRemote<vivaldi::mojom::VivaldiEncryptedMediaAccess> remote;
  interfaces->GetInterface(&remote);
  DCHECK(remote.is_bound());
  remote->NotifyEncryptedMediaAccess(key_system.Utf8());
}

}  // namespace vivaldi
