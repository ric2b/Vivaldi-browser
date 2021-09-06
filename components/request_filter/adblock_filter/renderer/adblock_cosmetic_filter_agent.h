// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_RENDERER_ADBLOCK_COSMETIC_FILTER_AGENT_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_RENDERER_ADBLOCK_COSMETIC_FILTER_AGENT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/request_filter/adblock_filter/renderer/blink/web_cosmetic_filter_client.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "components/request_filter/adblock_filter/mojom/adblock_cosmetic_filter.mojom.h"

namespace blink {
class RTCPeerConnectionHandler;
class WebLocalFrame;
}  // namespace blink

namespace adblock_filter {

class CosmeticFilterAgent
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<CosmeticFilterAgent>,
      public blink::WebCosmeticFilterClient {
 public:
  explicit CosmeticFilterAgent(content::RenderFrame* render_frame);
  ~CosmeticFilterAgent() override;

  void BlockWebRTCIfNeeded(
      base::WeakPtr<blink::RTCPeerConnectionHandler>
          rtc_peer_connection_handler,
      const webrtc::PeerConnectionInterface::RTCConfiguration& configuration)
      override;

 private:
  void OnDestruct() override;

  void DoBlockWebRTCIfNeeded(base::WeakPtr<blink::RTCPeerConnectionHandler>
                                 rtc_peer_connection_handler,
                             bool allowed);

  mojo::Remote<mojom::CosmeticFilter> cosmetic_filter_;

  DISALLOW_COPY_AND_ASSIGN(CosmeticFilterAgent);
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_RENDERER_ADBLOCK_COSMETIC_FILTER_AGENT_H_
