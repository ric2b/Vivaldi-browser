// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_COSMETIC_FILTER_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_COSMETIC_FILTER_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "third_party/webrtc/api/peer_connection_interface.h"

namespace blink {

class RTCPeerConnectionHandler;

class WebCosmeticFilterClient {
 public:
  virtual ~WebCosmeticFilterClient() = default;

  virtual void BlockWebRTCIfNeeded(
      base::WeakPtr<RTCPeerConnectionHandler> rtc_peer_connection_handler,
      const webrtc::PeerConnectionInterface::RTCConfiguration&
          configuration) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_COSMETIC_FILTER_CLIENT_H_
