// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_UTILS_H_
#define OSP_IMPL_QUIC_QUIC_UTILS_H_

#include "platform/base/ip_address.h"
#include "quiche/quic/platform/api/quic_socket_address.h"

namespace openscreen::osp {

quic::QuicIpAddress ToQuicIpAddress(const IPAddress& address);

quic::QuicSocketAddress ToQuicSocketAddress(const IPEndpoint& endpoint);

IPEndpoint ToIPEndpoint(const quic::QuicSocketAddress& address);

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_UTILS_H_
