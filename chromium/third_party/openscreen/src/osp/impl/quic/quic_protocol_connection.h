// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_PROTOCOL_CONNECTION_H_
#define OSP_IMPL_QUIC_QUIC_PROTOCOL_CONNECTION_H_

#include <cstdint>
#include <memory>

#include "osp/public/protocol_connection.h"

namespace openscreen::osp {

class QuicConnection;
class QuicStream;
class QuicStreamManager;

class QuicProtocolConnection final : public ProtocolConnection {
 public:
  class Owner {
   public:
    virtual ~Owner() = default;

    // Called right before `connection` is destroyed (destructor runs).
    virtual void OnConnectionDestroyed(QuicProtocolConnection& connection) = 0;
  };

  static std::unique_ptr<QuicProtocolConnection> FromExisting(
      Owner& owner,
      QuicConnection& connection,
      QuicStreamManager& manager,
      uint64_t instance_id);

  QuicProtocolConnection(Owner& owner,
                         QuicStream& stream,
                         uint64_t instance_id);
  QuicProtocolConnection(const QuicProtocolConnection&) = delete;
  QuicProtocolConnection& operator=(const QuicProtocolConnection&) = delete;
  QuicProtocolConnection(QuicProtocolConnection&&) noexcept = delete;
  QuicProtocolConnection& operator=(QuicProtocolConnection&&) noexcept = delete;
  ~QuicProtocolConnection() override;

  // ProtocolConnection overrides.
  uint64_t GetInstanceID() const override { return instance_id_; }
  uint64_t GetID() const override;
  void Write(ByteView bytes) override;
  void CloseWriteEnd() override;

  void OnClose();

 private:
  Owner& owner_;
  uint64_t instance_id_ = 0u;
  QuicStream* stream_ = nullptr;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_PROTOCOL_CONNECTION_H_
