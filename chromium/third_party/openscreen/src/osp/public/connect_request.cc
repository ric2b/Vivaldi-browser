// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/connect_request.h"

#include <utility>

#include "osp/public/protocol_connection_client.h"

namespace openscreen::osp {

ConnectRequestCallback::ConnectRequestCallback() = default;

ConnectRequestCallback::~ConnectRequestCallback() = default;

ConnectRequest::ConnectRequest() = default;

ConnectRequest::ConnectRequest(ProtocolConnectionClient* parent,
                               uint64_t request_id)
    : parent_(parent), request_id_(request_id) {}

ConnectRequest::ConnectRequest(ConnectRequest&& other) noexcept
    : parent_(other.parent_), request_id_(other.request_id_) {
  other.request_id_ = 0;
}
ConnectRequest& ConnectRequest::operator=(ConnectRequest&& other) noexcept {
  using std::swap;
  swap(parent_, other.parent_);
  swap(request_id_, other.request_id_);
  return *this;
}

ConnectRequest::~ConnectRequest() {
  if (request_id_)
    parent_->CancelConnectRequest(request_id_);
}

}  // namespace openscreen::osp
