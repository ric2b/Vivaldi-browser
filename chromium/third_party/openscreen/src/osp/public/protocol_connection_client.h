// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_PROTOCOL_CONNECTION_CLIENT_H_
#define OSP_PUBLIC_PROTOCOL_CONNECTION_CLIENT_H_

#include <memory>
#include <string>

#include "osp/public/protocol_connection_endpoint.h"
#include "osp/public/service_listener.h"

namespace openscreen::osp {

// Embedder's view of the network service that initiates OSP connections to OSP
// receivers.
class ProtocolConnectionClient : public ProtocolConnectionEndpoint,
                                 public ServiceListener::Observer {
 public:
  class ConnectionRequestCallback {
   public:
    ConnectionRequestCallback();
    ConnectionRequestCallback(const ConnectionRequestCallback&) = delete;
    ConnectionRequestCallback& operator=(const ConnectionRequestCallback&) =
        delete;
    ConnectionRequestCallback(ConnectionRequestCallback&&) noexcept = delete;
    ConnectionRequestCallback& operator=(ConnectionRequestCallback&&) noexcept =
        delete;
    virtual ~ConnectionRequestCallback();

    // Called when a new connection was created between 5-tuples.
    virtual void OnConnectionOpened(
        uint64_t request_id,
        std::unique_ptr<ProtocolConnection> connection) = 0;
    virtual void OnConnectionFailed(uint64_t request_id) = 0;
  };

  class ConnectRequest final {
   public:
    ConnectRequest();
    ConnectRequest(ProtocolConnectionClient* parent, uint64_t request_id);
    ConnectRequest(const ConnectRequest&) = delete;
    ConnectRequest& operator=(const ConnectRequest&) = delete;
    ConnectRequest(ConnectRequest&&) noexcept;
    ConnectRequest& operator=(ConnectRequest&&) noexcept;
    ~ConnectRequest();

    // This returns true for a valid and in progress ConnectRequest.
    // MarkComplete is called and this returns false when the request
    // completes.
    explicit operator bool() const { return request_id_; }

    uint64_t request_id() const { return request_id_; }

    // Records that the requested connect operation is complete so it doesn't
    // need to attempt a cancel on destruction.
    void MarkComplete() { request_id_ = 0; }

   private:
    ProtocolConnectionClient* parent_ = nullptr;
    // The `request_id_` of a valid ConnectRequest should be greater than 0.
    uint64_t request_id_ = 0;
  };

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
                       ConnectionRequestCallback* request_callback) = 0;

 protected:
  virtual void CancelConnectRequest(uint64_t request_id) = 0;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_PROTOCOL_CONNECTION_CLIENT_H_
