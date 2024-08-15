// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_CONSTANTS_H_
#define OSP_IMPL_QUIC_QUIC_CONSTANTS_H_

namespace openscreen::osp {

// ALPN required for QUIC connections made by OSP.
// Spec: https://w3c.github.io/openscreenprotocol/#tls-13
constexpr char kOpenScreenProtocolALPN[] = "osp";

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_CONSTANTS_H_
