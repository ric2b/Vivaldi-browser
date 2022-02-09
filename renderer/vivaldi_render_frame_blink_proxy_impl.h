// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef RENDERER_VIVALDI_RENDER_FRAME_BLINK_PROXY_IMPL_H_
#define RENDERER_VIVALDI_RENDER_FRAME_BLINK_PROXY_IMPL_H_

#include "services/service_manager/public/cpp/binder_registry.h"

#include "renderer/blink/vivaldi_render_frame_blink_proxy.h"

namespace content {
class RenderFrame;
}

class VivaldiRenderFrameBlinkProxyImpl : public VivaldiRenderFrameBlinkProxy {
 public:
  static void PrepareFrame(content::RenderFrame* render_frame,
                           service_manager::BinderRegistry* registry);

  vivaldi::mojom::blink::VivaldiFrameHostService* GetFrameHostService(
      blink::WebLocalFrame* web_frame) override;
  adblock_filter::mojom::blink::CosmeticFilter* GetCosmeticFilter(
      blink::WebLocalFrame* web_frame) override;

 private:
  static void InitProxy();
};

#endif  // RENDERER_VIVALDI_RENDER_FRAME_BLINK_PROXY_IMPL_H_
