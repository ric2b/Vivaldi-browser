// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_PROTOCOL_CONNECTION_H_
#define OSP_PUBLIC_PROTOCOL_CONNECTION_H_

#include <cstdint>
#include <type_traits>

#include "osp/msgs/osp_messages.h"
#include "platform/base/error.h"
#include "platform/base/span.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

template <typename T>
using MessageEncodingFunction =
    std::add_pointer_t<bool(const T&, msgs::CborEncodeBuffer*)>;

// Represents an embedder's view of a connection between an Open Screen
// controller and a receiver.  Both the controller and receiver will have a
// ProtocolConnection object, although the information known about the other
// party may not be symmetrical.
//
// A ProtocolConnection supports multiple protocols defined by the Open Screen
// standard and can be extended by embedders with additional protocols.
class ProtocolConnection {
 public:
  class Observer {
   public:
    Observer();
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    Observer(Observer&&) noexcept = delete;
    Observer& operator=(Observer&&) noexcept = delete;
    virtual ~Observer();

    // Called when `connection` is no longer available, either because the
    // underlying transport was terminated, the underying system resource was
    // closed, or data can no longer be exchanged.
    virtual void OnConnectionClosed(const ProtocolConnection& connection) = 0;
  };

  ProtocolConnection();
  ProtocolConnection(const ProtocolConnection&) = delete;
  ProtocolConnection& operator=(const ProtocolConnection&) = delete;
  ProtocolConnection(ProtocolConnection&&) noexcept = delete;
  ProtocolConnection& operator=(ProtocolConnection&&) noexcept = delete;
  virtual ~ProtocolConnection();

  // TODO(mfoltz): Define extension API exposed to embedders.  This would be
  // used, for example, to query for and implement vendor-specific protocols
  // alongside the Open Screen Protocol.

  // NOTE: ProtocolConnection instances that are owned by clients will have a
  // ServiceInfo attached with data from discovery and QUIC connection
  // establishment.  What about server connections?  We probably want to have
  // two different structures representing what the client and server know about
  // a connection.

  void SetObserver(Observer* observer);

  template <typename T>
  Error WriteMessage(const T& message, MessageEncodingFunction<T> encoder) {
    msgs::CborEncodeBuffer buffer;
    if (!encoder(message, &buffer)) {
      OSP_LOG_WARN << "failed to properly encode message";
      return Error::Code::kParseError;
    }

    Write(ByteView(buffer.data(), buffer.size()));
    return Error::None();
  }

  virtual uint64_t GetInstanceID() const = 0;
  virtual uint64_t GetID() const = 0;
  virtual void Write(ByteView bytes) = 0;
  virtual void Close() = 0;

 protected:
  Observer* observer_ = nullptr;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_PROTOCOL_CONNECTION_H_
