// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_RENDERER_BLINK_WEB_COSMETIC_FILTER_CLIENT_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_RENDERER_BLINK_WEB_COSMETIC_FILTER_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "third_party/webrtc/api/peer_connection_interface.h"

namespace blink {

class WebLocalFrame;
class RTCPeerConnectionHandler;

class WebCosmeticFilterClient {
 public:
  static void BlockWebRTCIfNeeded(
      WebLocalFrame* web_frame,
      base::WeakPtr<RTCPeerConnectionHandler> rtc_peer_connection_handler,
      const webrtc::PeerConnectionInterface::RTCConfiguration& configuration);
};

}  // namespace blink

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_RENDERER_BLINK_WEB_COSMETIC_FILTER_CLIENT_H_
