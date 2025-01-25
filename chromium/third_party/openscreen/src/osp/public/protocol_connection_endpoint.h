// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_PROTOCOL_CONNECTION_ENDPOINT_H_
#define OSP_PUBLIC_PROTOCOL_CONNECTION_ENDPOINT_H_

#include <memory>
#include <ostream>

#include "osp/public/instance_request_ids.h"
#include "osp/public/message_demuxer.h"
#include "osp/public/protocol_connection.h"

namespace openscreen::osp {

// There are two kinds of ProtocolConnectionEndpoints: ProtocolConnectionClient
// and ProtocolConnectionServer. They each define the special interfaces that
// the corresponding service needs to implement, while this class holds common
// interfaces for the two classes.
class ProtocolConnectionEndpoint {
 public:
  enum class State {
    kStopped = 0,
    kRunning,
    kSuspended,
  };

  ProtocolConnectionEndpoint();
  ProtocolConnectionEndpoint(const ProtocolConnectionEndpoint&) = delete;
  ProtocolConnectionEndpoint& operator=(const ProtocolConnectionEndpoint&) =
      delete;
  ProtocolConnectionEndpoint(ProtocolConnectionEndpoint&&) noexcept = delete;
  ProtocolConnectionEndpoint& operator=(ProtocolConnectionEndpoint&&) noexcept =
      delete;
  virtual ~ProtocolConnectionEndpoint();

  // Returns false if GetState() != kStopped, otherwise returns true and the
  // service will start.
  virtual bool Start() = 0;

  // Returns false if GetState() != (kRunning|kSuspended), otherwise returns
  // true and the service will stop.
  virtual bool Stop() = 0;

  // Returns false if GetState() != kRunning, otherwise returns true and the
  // service will suspend.
  virtual bool Suspend() = 0;

  // Returns false if GetState() != kSuspended, otherwise retruns ture and the
  // service will start again.
  virtual bool Resume() = 0;

  // Returns current state of the service.
  virtual State GetState() = 0;

  // Returns MessageDemuxer* used by the service.
  virtual MessageDemuxer& GetMessageDemuxer() = 0;

  // Returns InstanceRequestIds* used by the service.
  virtual InstanceRequestIds& GetInstanceRequestIds() = 0;

  // Synchronously open a new ProtocolConnection (corresponds to a underlying
  // QuicStream) to an instance identified by `instance_id`.  Returns nullptr if
  // it can't be completed synchronously (e.g. there are no existing open
  // connections to that instance).
  virtual std::unique_ptr<ProtocolConnection> CreateProtocolConnection(
      uint64_t instance_id) = 0;
};

std::ostream& operator<<(std::ostream& os,
                         ProtocolConnectionEndpoint::State state);

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_PROTOCOL_CONNECTION_ENDPOINT_H_
