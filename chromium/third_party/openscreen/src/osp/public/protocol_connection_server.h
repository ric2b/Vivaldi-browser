// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_PROTOCOL_CONNECTION_SERVER_H_
#define OSP_PUBLIC_PROTOCOL_CONNECTION_SERVER_H_

#include <string>

#include "osp/public/protocol_connection_endpoint.h"

namespace openscreen::osp {

// Embedder's view of the network service that receives OSP connections from OSP
// initiaters.
class ProtocolConnectionServer : public ProtocolConnectionEndpoint {
 public:
  ProtocolConnectionServer();
  ProtocolConnectionServer(const ProtocolConnectionServer&) = delete;
  ProtocolConnectionServer& operator=(const ProtocolConnectionServer&) = delete;
  ProtocolConnectionServer(ProtocolConnectionServer&&) noexcept = delete;
  ProtocolConnectionServer& operator=(ProtocolConnectionServer&&) noexcept =
      delete;
  ~ProtocolConnectionServer() override;

  // Returns the fingerprint of the server's certificate. The fingerprint is
  // sent to the client as a DNS TXT record. Client uses the fingerprint to
  // verify server's certificate.
  virtual std::string GetAgentFingerprint() = 0;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_PROTOCOL_CONNECTION_SERVER_H_
