// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_IMPL_MDNS_SENDER_H_
#define DISCOVERY_MDNS_IMPL_MDNS_SENDER_H_

#include "platform/api/udp_socket.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"

namespace openscreen::discovery {

class MdnsMessage;

class MdnsSender {
 public:
  // MdnsSender does not own `socket` and expects that its lifetime exceeds the
  // lifetime of MdnsSender.
  explicit MdnsSender(UdpSocket& socket);
  MdnsSender(const MdnsSender& other) = delete;
  MdnsSender(MdnsSender&& other) noexcept = delete;
  virtual ~MdnsSender();

  MdnsSender& operator=(const MdnsSender& other) = delete;
  MdnsSender& operator=(MdnsSender&& other) noexcept = delete;

  virtual Error SendMulticast(const MdnsMessage& message);
  virtual Error SendMessage(const MdnsMessage& message,
                            const IPEndpoint& endpoint);

  void OnSendError(UdpSocket* socket, const Error& error);

 private:
  UdpSocket& socket_;
};

}  // namespace openscreen::discovery

#endif  // DISCOVERY_MDNS_IMPL_MDNS_SENDER_H_
