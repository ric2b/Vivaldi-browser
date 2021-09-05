// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/renderer/adblock_cosmetic_filter_agent.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"

namespace adblock_filter {

CosmeticFilterAgent::CosmeticFilterAgent(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<CosmeticFilterAgent>(render_frame) {
  DCHECK(render_frame);
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      cosmetic_filter_.BindNewPipeAndPassReceiver());
  render_frame->GetWebFrame()->SetCosmeticFilterClient(this);
}

CosmeticFilterAgent::~CosmeticFilterAgent() = default;

// static
CosmeticFilterAgent* CosmeticFilterAgent::FromWebFrame(
    blink::WebLocalFrame* frame) {
  return Get(content::RenderFrame::FromWebFrame(frame));
}

void CosmeticFilterAgent::OnDestruct() {
  delete this;
}

void CosmeticFilterAgent::DidCreateNewDocument() {
  // Unretained is safe because cosmetic_filter_ cancels all callbacks when
  // destroyed.
  if (cosmetic_filter_)
    cosmetic_filter_->GetStyleSheet(
        render_frame()->GetWebFrame()->GetDocument().Url(),
        base::BindOnce(&CosmeticFilterAgent::InjectStyleSheet,
                       base::Unretained(this)));
}

void CosmeticFilterAgent::InjectStyleSheet(const std::string& stylesheet_code) {
  if (render_frame())
    render_frame()->GetWebFrame()->GetDocument().InsertStyleSheet(
        blink::WebString::FromUTF8(stylesheet_code), nullptr,
        blink::WebDocument::kUserOrigin);
}

void CosmeticFilterAgent::BlockWebRTCIfNeeded(
    base::WeakPtr<blink::RTCPeerConnectionHandler> rtc_peer_connection_handler,
    const webrtc::PeerConnectionInterface::RTCConfiguration& configuration) {
  if (!cosmetic_filter_)
    return;

  std::vector<GURL> ice_servers;
  for (const auto& server : configuration.servers) {
    if (!server.uri.empty())
      ice_servers.push_back(GURL(server.uri));
    for (const auto& url : server.urls)
      ice_servers.push_back(GURL(url));
  }

  cosmetic_filter_->ShouldAllowWebRTC(
      render_frame()->GetWebFrame()->GetDocument().Url(), ice_servers,
      base::BindOnce(&CosmeticFilterAgent::DoBlockWebRTCIfNeeded,
                     base::Unretained(this), rtc_peer_connection_handler));
}

void CosmeticFilterAgent::DoBlockWebRTCIfNeeded(
    base::WeakPtr<blink::RTCPeerConnectionHandler> rtc_peer_connection_handler,
    bool allowed) {
  if (!allowed)
    rtc_peer_connection_handler->CloseClientPeerConnection();
}

}  // namespace adblock_filter