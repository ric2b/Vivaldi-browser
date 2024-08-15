// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_PROTOCOL_CONNECTION_SERVER_FACTORY_H_
#define OSP_PUBLIC_PROTOCOL_CONNECTION_SERVER_FACTORY_H_

#include <memory>

#include "osp/public/endpoint_config.h"
#include "osp/public/protocol_connection_server.h"

namespace openscreen {

class TaskRunner;

namespace osp {

class ProtocolConnectionServerFactory {
 public:
  static std::unique_ptr<ProtocolConnectionServer> Create(
      const EndpointConfig& config,
      MessageDemuxer& demuxer,
      ProtocolConnectionServer::Observer& observer,
      TaskRunner& task_runner);
};

}  // namespace osp
}  // namespace openscreen

#endif  // OSP_PUBLIC_PROTOCOL_CONNECTION_SERVER_FACTORY_H_
