// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_PROTOCOL_CONNECTION_CLIENT_H_
#define OSP_PUBLIC_PROTOCOL_CONNECTION_CLIENT_H_

#include <string>

#include "osp/public/protocol_connection_endpoint.h"
#include "osp/public/service_listener.h"

namespace openscreen::osp {

class ConnectRequest;
class ConnectRequestCallback;

// Embedder's view of the network service that initiates OSP connections to OSP
// receivers.
class ProtocolConnectionClient : public ProtocolConnectionEndpoint,
                                 public ServiceListener::Observer {
 public:
  ProtocolConnectionClient();
  ProtocolConnectionClient(const ProtocolConnectionClient&) = delete;
  ProtocolConnectionClient& operator=(const ProtocolConnectionClient&) = delete;
  ProtocolConnectionClient(ProtocolConnectionClient&&) noexcept = delete;
  ProtocolConnectionClient& operator=(ProtocolConnectionClient&&) noexcept =
      delete;
  ~ProtocolConnectionClient() override;

  // Open a new connection to `instance_name`.  This may succeed synchronously
  // if there are already connections open to `instance_name`, otherwise it will
  // be asynchronous. Returns true if succeed synchronously or asynchronously,
  // false otherwise. `request` is overwritten with the result of a successful
  // connection attempt.
  virtual bool Connect(std::string_view instance_name,
                       ConnectRequest& request,
                       ConnectRequestCallback* request_callback) = 0;

  virtual void CancelConnectRequest(uint64_t request_id) = 0;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_PROTOCOL_CONNECTION_CLIENT_H_
