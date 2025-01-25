// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_connection_factory_base.h"

#include <utility>

#include "osp/impl/quic/quic_alarm_factory_impl.h"
#include "quiche/quic/core/quic_default_clock.h"
#include "quiche/quic/core/quic_default_connection_helper.h"

namespace openscreen::osp {

QuicConnectionFactoryBase::QuicConnectionFactoryBase(TaskRunner& task_runner)
    : task_runner_(task_runner) {
  helper_ = std::make_unique<quic::QuicDefaultConnectionHelper>();
  alarm_factory_ = std::make_unique<QuicAlarmFactoryImpl>(
      task_runner, quic::QuicDefaultClock::Get());
}

QuicConnectionFactoryBase::~QuicConnectionFactoryBase() {
  for (auto& it : connections_) {
    it.second.connection->Close();
  }
}

void QuicConnectionFactoryBase::OnError(UdpSocket* socket, const Error& error) {
  // Close all related connections and the UpdSocket will be closed because none
  // of the remaining `connections_` reference the socket.
  for (auto& it : connections_) {
    if (it.second.socket == socket) {
      it.second.connection->Close();
    }
  }
}

void QuicConnectionFactoryBase::OnSendError(UdpSocket* socket,
                                            const Error& error) {
  OnError(socket, error);
}

}  // namespace openscreen::osp
