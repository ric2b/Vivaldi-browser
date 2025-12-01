// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_OSP_CONSTANTS_H_
#define OSP_PUBLIC_OSP_CONSTANTS_H_

namespace openscreen::osp {

// See https://w3c.github.io/openscreenprotocol/network.html#discovery for the
// definitions of these values.

inline constexpr char kFingerprint[] = "fp";
inline constexpr char kAuthToken[] = "at";
inline constexpr char kMetadataValue[] = "mv";

inline constexpr char kOpenScreenServiceName[] = "_openscreen._udp";
inline constexpr char kDnsSdDomainId[] = "local";
inline constexpr char kOpenScreenServiceType[] = "_openscreen._udp.local";

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_OSP_CONSTANTS_H_
