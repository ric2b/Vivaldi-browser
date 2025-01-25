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
  ~FakeQuicStream() override;

  void ReceiveData(ByteView bytes);
  void CloseReadEnd();

  std::vector<uint8_t> TakeReceivedData();
  std::vector<uint8_t> TakeWrittenData();

  bool both_ends_closed() const {
    return write_end_closed_ && read_end_closed_;
  }
  bool write_end_closed() const { return write_end_closed_; }
  bool read_end_closed() const { return read_end_closed_; }

  Delegate& delegate() { return delegate_; }

  uint64_t GetStreamId() override;
  void Write(ByteView bytes) override;
  void CloseWriteEnd() override;

 private:
  uint64_t stream_id_ = 0u;
  bool write_end_closed_ = false;
  bool read_end_closed_ = false;
  std::vector<uint8_t> write_buffer_;
  std::vector<uint8_t> read_buffer_;
};

class FakeQuicConnection final : public QuicConnection {
 public:
  FakeQuicConnection(std::string_view instance_name,
                     FakeQuicConnectionFactoryBridge& parent_factory,
                     Delegate& delegate);
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
