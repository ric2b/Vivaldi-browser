// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/protocol_connection.h"

namespace openscreen::osp {

ProtocolConnection::Observer::Observer() = default;
ProtocolConnection::Observer::~Observer() = default;

ProtocolConnection::ProtocolConnection() = default;
ProtocolConnection::~ProtocolConnection() = default;

void ProtocolConnection::SetObserver(Observer* observer) {
  OSP_CHECK(!observer_ || !observer);
  observer_ = observer;
}

}  // namespace openscreen::osp
