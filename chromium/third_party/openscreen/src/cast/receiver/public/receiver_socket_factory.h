// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_RECEIVER_PUBLIC_RECEIVER_SOCKET_FACTORY_H_
#define CAST_RECEIVER_PUBLIC_RECEIVER_SOCKET_FACTORY_H_

#include <memory>
#include <vector>

#include "cast/common/public/cast_socket.h"
#include "platform/api/tls_connection_factory.h"
#include "platform/base/ip_address.h"

namespace openscreen::cast {

class ReceiverSocketFactory final : public TlsConnectionFactory::Client {
 public:
  class Client {
   public:
    virtual void OnConnected(ReceiverSocketFactory* factory,
                             const IPEndpoint& endpoint,
                             std::unique_ptr<CastSocket> socket) = 0;
    virtual void OnError(ReceiverSocketFactory* factory,
                         const Error& error) = 0;

   protected:
    virtual ~Client();
  };

  // `client` and `socket_client` must outlive `this`.
  // TODO(btolsch): Add TaskRunner argument just for sequence checking.
  ReceiverSocketFactory(Client& client, CastSocket::Client& socket_client);
  ~ReceiverSocketFactory();

  // TlsConnectionFactory::Client overrides.
  void OnAccepted(TlsConnectionFactory* factory,
                  std::vector<uint8_t> der_x509_peer_cert,
                  std::unique_ptr<TlsConnection> connection) override;
  void OnConnected(TlsConnectionFactory* factory,
                   std::vector<uint8_t> der_x509_peer_cert,
                   std::unique_ptr<TlsConnection> connection) override;
  void OnConnectionFailed(TlsConnectionFactory* factory,
                          const IPEndpoint& remote_address) override;
  void OnError(TlsConnectionFactory* factory, const Error& error) override;

 private:
  Client& client_;
  CastSocket::Client& socket_client_;
};

}  // namespace openscreen::cast

#endif  // CAST_RECEIVER_PUBLIC_RECEIVER_SOCKET_FACTORY_H_
