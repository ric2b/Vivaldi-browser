// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/renderer/blink/web_cosmetic_filter_client.h"

#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"

#include "components/request_filter/adblock_filter/mojom/adblock_cosmetic_filter.mojom-blink.h"
#include "renderer/blink/vivaldi_render_frame_blink_proxy.h"

namespace blink {

// static
void WebCosmeticFilterClient::BlockWebRTCIfNeeded(
    WebLocalFrame* web_frame,
    base::WeakPtr<blink::RTCPeerConnectionHandler> rtc_peer_connection_handler,
    const webrtc::PeerConnectionInterface::RTCConfiguration& configuration) {
  DCHECK(rtc_peer_connection_handler);
  VivaldiRenderFrameBlinkProxy* proxy =
      VivaldiRenderFrameBlinkProxy::GetProxy();
  if (!proxy)
    return;
  adblock_filter::mojom::blink::CosmeticFilter* cosmetic_filter =
      proxy->GetCosmeticFilter(web_frame);
  if (!cosmetic_filter)
    return;

  WTF::Vector<KURL> ice_servers;
  for (const auto& server : configuration.servers) {
    if (!server.uri.empty())
      ice_servers.push_back(KURL(GURL(server.uri)));
    for (const auto& url : server.urls)
      ice_servers.push_back(KURL(GURL(url)));
  }

  auto process_reply = [](base::WeakPtr<blink::RTCPeerConnectionHandler>
                              rtc_peer_connection_handler,
                          bool allowed) {
    if (!allowed && rtc_peer_connection_handler) {
      rtc_peer_connection_handler->CloseClientPeerConnection();
    }
  };

  cosmetic_filter->ShouldAllowWebRTC(
      web_frame->GetDocument().Url(), ice_servers,
      base::BindOnce(process_reply, std::move(rtc_peer_connection_handler)));
}

}  // namespace blink
