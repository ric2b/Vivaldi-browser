// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_ENDPOINT_CONFIG_H_
#define OSP_PUBLIC_ENDPOINT_CONFIG_H_

#include <vector>

#include "platform/base/ip_address.h"

namespace openscreen::osp {

struct EndpointConfig {
  // The list of connection endpoints that are advertised for Open Screen
  // protocol connections.
  std::vector<IPEndpoint> connection_endpoints;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_ENDPOINT_CONFIG_H_
