// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "renderer/blink/vivaldi_render_frame_blink_proxy.h"

#ifndef BLINK_MOJO_IMPL
#define BLINK_MOJO_IMPL 1
#endif

#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"

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
#if !BUILDFLAG(IS_ANDROID)
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

/* static */
void VivaldiRenderFrameBlinkProxy::DidChangeLoadProgressExtended(
    blink::LocalFrame* local_frame,
    int64_t loaded_bytes_delta,
    int loaded_resource_delta,
    int total_resource_delta) {
#if !BUILDFLAG(IS_ANDROID)
  if (!g_proxy_singleton)
    return;
  blink::WebLocalFrame* web_frame =
      blink::WebLocalFrameImpl::FromFrame(local_frame);
  if (!web_frame)
    return;
  vivaldi::mojom::blink::VivaldiFrameHostService* frame_host_service =
      g_proxy_singleton->GetFrameHostService(web_frame);
  if (!frame_host_service)
    return;
  frame_host_service->DidChangeLoadProgressExtended(
      loaded_bytes_delta, loaded_resource_delta, total_resource_delta);
#endif
}
