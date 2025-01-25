// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/protocol_connection_client.h"

#include <utility>

#include "util/osp_logging.h"

namespace openscreen::osp {

ProtocolConnectionClient::ConnectionRequestCallback::
    ConnectionRequestCallback() = default;

ProtocolConnectionClient::ConnectionRequestCallback::
    ~ConnectionRequestCallback() = default;

ProtocolConnectionClient::ConnectRequest::ConnectRequest() = default;

ProtocolConnectionClient::ConnectRequest::ConnectRequest(
    ProtocolConnectionClient* parent,
    uint64_t request_id)
    : parent_(parent), request_id_(request_id) {}

ProtocolConnectionClient::ConnectRequest::ConnectRequest(
    ConnectRequest&& other) noexcept
    : parent_(other.parent_), request_id_(other.request_id_) {
  other.request_id_ = 0;
}

ProtocolConnectionClient::ConnectRequest&
ProtocolConnectionClient::ConnectRequest::operator=(
    ConnectRequest&& other) noexcept {
  using std::swap;
  swap(parent_, other.parent_);
  swap(request_id_, other.request_id_);
  return *this;
}

ProtocolConnectionClient::ConnectRequest::~ConnectRequest() {
  if (request_id_)
    parent_->CancelConnectRequest(request_id_);
}

ProtocolConnectionClient::ProtocolConnectionClient() = default;

ProtocolConnectionClient::~ProtocolConnectionClient() = default;

}  // namespace openscreen::osp
