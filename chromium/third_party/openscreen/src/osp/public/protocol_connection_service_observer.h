// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_PROTOCOL_CONNECTION_SERVICE_OBSERVER_H_
#define OSP_PUBLIC_PROTOCOL_CONNECTION_SERVICE_OBSERVER_H_

#include <memory>

#include "osp/public/network_metrics.h"
#include "osp/public/protocol_connection.h"
#include "platform/base/error.h"

namespace openscreen::osp {

class ProtocolConnectionServiceObserver {
 public:
  ProtocolConnectionServiceObserver() = default;
  ProtocolConnectionServiceObserver(const ProtocolConnectionServiceObserver&) =
      delete;
  ProtocolConnectionServiceObserver& operator=(
      const ProtocolConnectionServiceObserver&) = delete;
  ProtocolConnectionServiceObserver(
      ProtocolConnectionServiceObserver&&) noexcept = delete;
  ProtocolConnectionServiceObserver& operator=(
      ProtocolConnectionServiceObserver&&) noexcept = delete;
  virtual ~ProtocolConnectionServiceObserver() = default;

  // Called when the state becomes kRunning.
  virtual void OnRunning() = 0;
  // Called when the state becomes kStopped.
  virtual void OnStopped() = 0;
  // Called when the state becomes kSuspended. Only ProtocolConnectionServer
  // supports kSuspended state now.
  virtual void OnSuspended() = 0;

  // Called when metrics have been collected by the service.
  virtual void OnMetrics(const NetworkMetrics& metrics) = 0;
  // Called when an error has occurred.
  virtual void OnError(const Error& error) = 0;

  virtual void OnIncomingConnection(
      std::unique_ptr<ProtocolConnection> connection) = 0;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_PROTOCOL_CONNECTION_SERVICE_OBSERVER_H_
