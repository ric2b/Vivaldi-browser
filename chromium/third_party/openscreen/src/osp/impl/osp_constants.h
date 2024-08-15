// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_OSP_CONSTANTS_H_
#define OSP_IMPL_OSP_CONSTANTS_H_

namespace openscreen::osp {

// Spec: https://w3c.github.io/openscreenprotocol/#agent-fingerprint
constexpr char kFingerprint[] = "fp";

// Spec: https://w3c.github.io/openscreenprotocol/#tls-13
constexpr char kOpenScreenServiceName[] = "_openscreen._udp";

// Spec: https://w3c.github.io/openscreenprotocol/#advertising-agent
constexpr char kDnsSdDomainId[] = "local";

constexpr char kFriendlyNameTxtKey[] = "fn";

}  // namespace openscreen::osp

#endif  // OSP_IMPL_OSP_CONSTANTS_H_
