// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_CONSTANTS_H_
#define OSP_IMPL_QUIC_QUIC_CONSTANTS_H_

#include <cstddef>

namespace openscreen::osp {

// ALPN required for QUIC connections made by OSP.
// Spec: https://w3c.github.io/openscreenprotocol/#tls-13
constexpr char kOpenScreenProtocolALPN[] = "osp";

// Secret key material used for encrypting and decrypting source address tokens.
// It can be of any length as it is fed into a KDF before use.
constexpr char kSourceAddressTokenSecret[] = "secret";

// A counter of how many new sessions can be create by QuicDispatcher within
// current event loop. When reaches 0, it means can't create sessions for now.
constexpr size_t kMaxConnectionsToCreate = 256;

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_CONSTANTS_H_
