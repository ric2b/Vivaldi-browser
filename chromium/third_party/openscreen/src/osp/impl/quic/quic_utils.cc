// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_utils.h"

#include <stdint.h>

#include "quiche/common/quiche_ip_address.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

quic::QuicIpAddress ToQuicIpAddress(const IPAddress& address) {
  if (address.IsV4()) {
    uint8_t address_8[IPAddress::kV4Size];
    address.CopyToV4(address_8);
    const uint32_t address_32 = (address_8[3] << 24) + (address_8[2] << 16) +
                                (address_8[1] << 8) + (address_8[0]);
    const in_addr result{address_32};
    static_assert(sizeof(result) == IPAddress::kV4Size,
                  "Address size mismatch");
    return quic::QuicIpAddress(result);
  }

  if (address.IsV6()) {
    in6_addr result;
    address.CopyToV6(result.s6_addr);
    static_assert(sizeof(result) == IPAddress::kV6Size,
                  "Address size mismatch");
    return quic::QuicIpAddress(result);
  }

  return quic::QuicIpAddress();
}

quic::QuicSocketAddress ToQuicSocketAddress(const IPEndpoint& endpoint) {
  return quic::QuicSocketAddress(ToQuicIpAddress(endpoint.address),
                                 endpoint.port);
}

IPEndpoint ToIPEndpoint(const quic::QuicSocketAddress& address) {
  auto result = IPEndpoint::Parse(address.ToString());
  return result.value(IPEndpoint{});
}

}  // namespace openscreen::osp
