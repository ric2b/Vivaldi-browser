// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef RENDERER_BLINK_VIVALDI_RENDER_FRAME_BLINK_PROXY_H_
#define RENDERER_BLINK_VIVALDI_RENDER_FRAME_BLINK_PROXY_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"

#include "components/request_filter/adblock_filter/mojom/adblock_cosmetic_filter.mojom-blink-forward.h"
#include "renderer/mojo/vivaldi_frame_host_service.mojom-blink-forward.h"

namespace blink {
class WebLocalFrame;
class WebMediaPlayer;
}  // namespace blink

// Proxy to access functionality that depends on content::RenderFrame from
// Vivaldi patches to blink core like mojo remotes to services on the browser
// side.
//
// Only single instance of a proxy implementation should be constructed. It
// should never be destructed. Use base::NoDestruct<> to ensure that.
class CORE_EXPORT VivaldiRenderFrameBlinkProxy {
 public:
  // Register the proxy instance as the global singleton.
  VivaldiRenderFrameBlinkProxy();

  static VivaldiRenderFrameBlinkProxy* GetProxy();
  static void SendMediaElementAddedEvent(blink::WebMediaPlayer* player);

  virtual vivaldi::mojom::blink::VivaldiFrameHostService* GetFrameHostService(
      blink::WebLocalFrame* web_frame) = 0;

  virtual adblock_filter::mojom::blink::CosmeticFilter* GetCosmeticFilter(
      blink::WebLocalFrame* web_frame) = 0;

 protected:
  virtual ~VivaldiRenderFrameBlinkProxy();

 private:
  DISALLOW_COPY_AND_ASSIGN(VivaldiRenderFrameBlinkProxy);
};

#endif  // RENDERER_BLINK_VIVALDI_RENDER_FRAME_BLINK_PROXY_H_
