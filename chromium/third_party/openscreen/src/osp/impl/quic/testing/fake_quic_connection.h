// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_TESTING_FAKE_QUIC_CONNECTION_H_
#define OSP_IMPL_QUIC_TESTING_FAKE_QUIC_CONNECTION_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "osp/impl/quic/quic_connection.h"

namespace openscreen::osp {

class FakeQuicConnectionFactoryBridge;

class FakeQuicStream final : public QuicStream {
 public:
  FakeQuicStream(Delegate& delegate, uint64_t id);
  FakeQuicStream(const FakeQuicStream&) = delete;
  FakeQuicStream& operator=(const FakeQuicStream&) = delete;
  FakeQuicStream(FakeQuicStream&&) noexcept = delete;
  FakeQuicStream& operator=(FakeQuicStream&&) noexcept = delete;
  ~FakeQuicStream() override;

  void ReceiveData(ByteView bytes);

  std::vector<uint8_t> TakeReceivedData();
  std::vector<uint8_t> TakeWrittenData();

  bool is_closed() const { return is_closed_; }
  Delegate& delegate() { return delegate_; }

  uint64_t GetStreamId() override { return stream_id_; }
  void Write(ByteView bytes) override;
  void Close() override;

 private:
  uint64_t stream_id_ = 0u;
  bool is_closed_ = false;
  std::vector<uint8_t> write_buffer_;
  std::vector<uint8_t> read_buffer_;
};

class FakeQuicConnection final : public QuicConnection {
 public:
  FakeQuicConnection(std::string_view instance_name,
                     FakeQuicConnectionFactoryBridge& parent_factory,
                     Delegate& delegate);
  FakeQuicConnection(const FakeQuicConnection&) = delete;
  FakeQuicConnection& operator=(const FakeQuicConnection&) = delete;
  FakeQuicConnection(FakeQuicConnection&&) noexcept = delete;
  FakeQuicConnection& operator=(FakeQuicConnection&&) noexcept = delete;
  ~FakeQuicConnection() override;

  Delegate& delegate() { return delegate_; }
  std::map<uint64_t, std::unique_ptr<FakeQuicStream>>& streams() {
    return streams_;
  }

  void OnCryptoHandshakeComplete();
  FakeQuicStream* MakeIncomingStream();

  // QuicConnection overrides.
  void OnPacketReceived(const UdpPacket& packet) override;
  QuicStream* MakeOutgoingStream(QuicStream::Delegate& delegate) override;
  void Close() override;

 private:
  FakeQuicConnectionFactoryBridge& parent_factory_;
  uint64_t next_stream_id_ = 1;
  std::map<uint64_t, std::unique_ptr<FakeQuicStream>> streams_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_TESTING_FAKE_QUIC_CONNECTION_H_
