// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "renderer/blink/vivaldi_render_frame_blink_proxy.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/web_media_player.h"

#include "app/vivaldi_apptools.h"
#include "renderer/mojo/vivaldi_frame_host_service.mojom-blink.h"

namespace {
VivaldiRenderFrameBlinkProxy* g_proxy_singleton = nullptr;
}

VivaldiRenderFrameBlinkProxy::VivaldiRenderFrameBlinkProxy() {
  DCHECK(!g_proxy_singleton);
  g_proxy_singleton = this;
}

VivaldiRenderFrameBlinkProxy::~VivaldiRenderFrameBlinkProxy() {
  // Implementation of the proxy should never be destructed.
  NOTREACHED();
}

/* static */
VivaldiRenderFrameBlinkProxy* VivaldiRenderFrameBlinkProxy::GetProxy() {
  return g_proxy_singleton;
}

/* static */
void VivaldiRenderFrameBlinkProxy::SendMediaElementAddedEvent(
    blink::WebMediaPlayer* player) {
#if !defined(OS_ANDROID)
  if (!vivaldi::IsVivaldiRunning())
    return;
  if (!player || !g_proxy_singleton)
    return;
  blink::WebLocalFrame* web_frame = player->VivaldiGetOwnerWebFrame();
  if (!web_frame)
    return;
  vivaldi::mojom::blink::VivaldiFrameHostService* frame_host_service =
      g_proxy_singleton->GetFrameHostService(web_frame);
  if (!frame_host_service)
    return;
  frame_host_service->NotifyMediaElementAdded();
#endif
}