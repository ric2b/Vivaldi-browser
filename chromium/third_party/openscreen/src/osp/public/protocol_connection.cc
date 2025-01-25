// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/protocol_connection.h"

namespace openscreen::osp {

ProtocolConnection::ProtocolConnection(uint64_t instance_id,
                                       uint64_t protocol_connection_id)
    : instance_id_(instance_id), id_(protocol_connection_id) {}

void ProtocolConnection::SetObserver(Observer* observer) {
  OSP_CHECK(!observer_ || !observer);
  observer_ = observer;
}

}  // namespace openscreen::osp
