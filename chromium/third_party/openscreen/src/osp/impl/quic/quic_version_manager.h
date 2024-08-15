// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_VERSION_MANAGER_H_
#define OSP_IMPL_QUIC_QUIC_VERSION_MANAGER_H_

#include "quiche/quic/core/quic_version_manager.h"
#include "quiche/quic/core/quic_versions.h"

namespace openscreen::osp {

class QuicVersionManager : public quic::QuicVersionManager {
 public:
  explicit QuicVersionManager(quic::ParsedQuicVersionVector supported_versions);
  QuicVersionManager(const QuicVersionManager&) = delete;
  QuicVersionManager& operator=(const QuicVersionManager&) = delete;
  QuicVersionManager(QuicVersionManager&&) noexcept = delete;
  QuicVersionManager& operator=(QuicVersionManager&&) noexcept = delete;
  ~QuicVersionManager() override;

  // This adds the custom ALPN to offer.
  void RefilterSupportedVersions() override;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_VERSION_MANAGER_H_
