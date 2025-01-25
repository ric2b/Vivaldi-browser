// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_PROTOCOL_CONNECTION_CLIENT_FACTORY_H_
#define OSP_PUBLIC_PROTOCOL_CONNECTION_CLIENT_FACTORY_H_

#include <memory>

#include "osp/public/protocol_connection_client.h"
#include "osp/public/protocol_connection_service_observer.h"
#include "osp/public/service_config.h"

namespace openscreen {

class TaskRunner;

namespace osp {

class ProtocolConnectionClientFactory {
 public:
  static std::unique_ptr<ProtocolConnectionClient> Create(
      const ServiceConfig& config,
      ProtocolConnectionServiceObserver& observer,
      TaskRunner& task_runner,
      size_t buffer_limit);
};

}  // namespace osp
}  // namespace openscreen

#endif  // OSP_PUBLIC_PROTOCOL_CONNECTION_CLIENT_FACTORY_H_
