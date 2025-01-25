// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/protocol_connection_server_factory.h"

#include "osp/impl/quic/quic_connection_factory_server.h"
#include "osp/impl/quic/quic_server.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"

namespace openscreen::osp {

// static
std::unique_ptr<ProtocolConnectionServer>
ProtocolConnectionServerFactory::Create(
    const ServiceConfig& config,
    ProtocolConnectionServiceObserver& observer,
    TaskRunner& task_runner,
    size_t buffer_limit) {
  return std::make_unique<QuicServer>(
      config, std::make_unique<QuicConnectionFactoryServer>(task_runner),
      observer, &Clock::now, task_runner, buffer_limit);
}

}  // namespace openscreen::osp
