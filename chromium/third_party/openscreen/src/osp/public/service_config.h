// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_SERVICE_CONFIG_H_
#define OSP_PUBLIC_SERVICE_CONFIG_H_

#include <string>
#include <vector>

#include "platform/base/ip_address.h"

namespace openscreen::osp {

struct ServiceConfig {
  // The list of connection endpoints that are advertised for Open Screen
  // protocol connections.
  std::vector<IPEndpoint> connection_endpoints;

  // This is empty for QuicClient and is only used by QuicServer.
  std::string instance_name;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_SERVICE_CONFIG_H_
