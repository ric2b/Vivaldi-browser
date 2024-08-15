// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_SERVICE_COMMON_H_
#define OSP_IMPL_QUIC_QUIC_SERVICE_COMMON_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "osp/impl/quic/quic_connection.h"
#include "osp/impl/quic/quic_stream.h"
#include "osp/public/protocol_connection.h"

namespace openscreen::osp {

class ServiceConnectionDelegate;

class QuicProtocolConnection final : public ProtocolConnection {
 public:
  class Owner {
   public:
    virtual ~Owner() = default;

    // Called right before |connection| is destroyed (destructor runs).
    virtual void OnConnectionDestroyed(QuicProtocolConnection* connection) = 0;
  };

  static std::unique_ptr<QuicProtocolConnection> FromExisting(
      Owner& owner,
      QuicConnection* connection,
      ServiceConnectionDelegate* delegate,
      uint64_t endpoint_id);

  QuicProtocolConnection(Owner& owner,
                         uint64_t endpoint_id,
                         uint64_t connection_id);
  ~QuicProtocolConnection() override;

  // ProtocolConnection overrides.
  void Write(const ByteView& bytes) override;
  void CloseWriteEnd() override;

  QuicStream* stream() { return stream_; }
  void set_stream(QuicStream* stream) { stream_ = stream; }

  void OnClose();

 private:
  Owner& owner_;
  QuicStream* stream_ = nullptr;
};

struct ServiceStreamPair {
  QuicStream* stream = nullptr;
  uint64_t protocol_connection_id = 0u;
  QuicProtocolConnection* protocol_connection = nullptr;
};

class ServiceConnectionDelegate final : public QuicConnection::Delegate,
                                        public QuicStream::Delegate {
 public:
  class ServiceDelegate : public QuicProtocolConnection::Owner {
   public:
    ~ServiceDelegate() override = default;

    virtual uint64_t OnCryptoHandshakeComplete(
        ServiceConnectionDelegate* delegate,
        std::string connection_id) = 0;
    virtual void OnIncomingStream(
        std::unique_ptr<QuicProtocolConnection> connection) = 0;
    virtual void OnConnectionClosed(uint64_t endpoint_id,
                                    std::string connection_id) = 0;
    virtual void OnDataReceived(uint64_t endpoint_id,
                                uint64_t protocol_connection_id,
                                const ByteView& bytes) = 0;
  };

  ServiceConnectionDelegate(ServiceDelegate& parent,
                            const IPEndpoint& endpoint);
  ~ServiceConnectionDelegate() override;

  void AddStreamPair(const ServiceStreamPair& stream_pair);
  void DropProtocolConnection(QuicProtocolConnection* connection);

  // This should be called at the end of each event loop that effects this
  // connection so streams that were closed by the other endpoint can be
  // destroyed properly.
  void DestroyClosedStreams();

  const IPEndpoint& endpoint() const { return endpoint_; }

  bool has_streams() const { return !streams_.empty(); }

  // QuicConnection::Delegate overrides.
  void OnCryptoHandshakeComplete(const std::string& connection_id) override;
  void OnIncomingStream(const std::string& connection_id,
                        QuicStream* stream) override;
  void OnConnectionClosed(const std::string& connection_id) override;
  QuicStream::Delegate& NextStreamDelegate(const std::string& connection_id,
                                           uint64_t stream_id) override;

  // QuicStream::Delegate overrides.
  void OnReceived(QuicStream* stream, const ByteView& bytes) override;
  void OnClose(uint64_t stream_id) override;

 private:
  ServiceDelegate& parent_;
  IPEndpoint endpoint_;
  uint64_t endpoint_id_;
  std::unique_ptr<QuicProtocolConnection> pending_connection_;
  std::map<uint64_t, ServiceStreamPair> streams_;
  std::vector<ServiceStreamPair> closed_streams_;
};

struct ServiceConnectionData {
  ServiceConnectionData(std::unique_ptr<QuicConnection> connection,
                        std::unique_ptr<ServiceConnectionDelegate> delegate);
  ServiceConnectionData(ServiceConnectionData&&) noexcept;
  ~ServiceConnectionData();
  ServiceConnectionData& operator=(ServiceConnectionData&&) noexcept;

  std::unique_ptr<QuicConnection> connection;
  std::unique_ptr<ServiceConnectionDelegate> delegate;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_SERVICE_COMMON_H_
