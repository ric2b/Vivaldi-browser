// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/testing/fake_quic_connection.h"

#include <memory>
#include <utility>

#include "osp/impl/quic/testing/fake_quic_connection_factory.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

FakeQuicStream::FakeQuicStream(Delegate& delegate, uint64_t id)
    : QuicStream(delegate), stream_id_(id) {}

FakeQuicStream::~FakeQuicStream() = default;

void FakeQuicStream::ReceiveData(ByteView bytes) {
  OSP_CHECK(!read_end_closed_);
  read_buffer_.insert(read_buffer_.end(), bytes.data(),
                      bytes.data() + bytes.size());
}

void FakeQuicStream::CloseReadEnd() {
  read_end_closed_ = true;
}

std::vector<uint8_t> FakeQuicStream::TakeReceivedData() {
  return std::move(read_buffer_);
}

std::vector<uint8_t> FakeQuicStream::TakeWrittenData() {
  return std::move(write_buffer_);
}

uint64_t FakeQuicStream::GetStreamId() {
  return stream_id_;
}

void FakeQuicStream::Write(ByteView bytes) {
  OSP_CHECK(!write_end_closed_);
  write_buffer_.insert(write_buffer_.end(), bytes.data(),
                       bytes.data() + bytes.size());
}

void FakeQuicStream::CloseWriteEnd() {
  write_end_closed_ = true;
}

FakeQuicConnection::FakeQuicConnection(
    std::string_view instance_name,
    FakeQuicConnectionFactoryBridge& parent_factory,
    Delegate& delegate)
    : QuicConnection(instance_name, delegate),
      parent_factory_(parent_factory) {}

FakeQuicConnection::~FakeQuicConnection() = default;

void FakeQuicConnection::OnCryptoHandshakeComplete() {
  instance_id_ = delegate_.OnCryptoHandshakeComplete(instance_name_);
}

FakeQuicStream* FakeQuicConnection::MakeIncomingStream() {
  uint64_t stream_id = next_stream_id_++;
  auto result = std::make_unique<FakeQuicStream>(
      delegate().GetStreamDelegate(instance_id_), stream_id);
  auto* stream_ptr = result.get();
  streams_.emplace(result->GetStreamId(), std::move(result));
  return stream_ptr;
}

void FakeQuicConnection::OnPacketReceived(const UdpPacket& packet) {
  OSP_NOTREACHED();
}

QuicStream* FakeQuicConnection::MakeOutgoingStream(
    QuicStream::Delegate& delegate) {
  auto result = std::make_unique<FakeQuicStream>(delegate, next_stream_id_++);
  auto* stream_ptr = result.get();
  streams_.emplace(result->GetStreamId(), std::move(result));
  parent_factory_.OnOutgoingStream(this, stream_ptr);
  return stream_ptr;
}

void FakeQuicConnection::Close() {
  parent_factory_.OnConnectionClosed(this);
  delegate().OnConnectionClosed(instance_id_);
  for (auto& stream : streams_) {
    stream.second->delegate().OnClose(stream.first);
    stream.second->delegate().OnReceived(stream.second.get(),
                                         ByteView(nullptr, size_t(0)));
  }
}

}  // namespace openscreen::osp
