// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_CONNECT_REQUEST_H_
#define OSP_PUBLIC_CONNECT_REQUEST_H_

#include <cstdint>

namespace openscreen::osp {

class ProtocolConnectionClient;

class ConnectRequestCallback {
 public:
  ConnectRequestCallback();
  ConnectRequestCallback(const ConnectRequestCallback&) = delete;
  ConnectRequestCallback& operator=(const ConnectRequestCallback&) = delete;
  ConnectRequestCallback(ConnectRequestCallback&&) noexcept = delete;
  ConnectRequestCallback& operator=(ConnectRequestCallback&&) noexcept = delete;
  virtual ~ConnectRequestCallback();

  // Called when a new connection (corresponds to a underlying QuicConnection)
  // was created between 5-tuples.
  virtual void OnConnectSucceed(uint64_t request_id, uint64_t instance_id) = 0;
  virtual void OnConnectFailed(uint64_t request_id) = 0;
};

class ConnectRequest final {
 public:
  ConnectRequest();
  ConnectRequest(ProtocolConnectionClient* parent, uint64_t request_id);
  ConnectRequest(const ConnectRequest&) = delete;
  ConnectRequest& operator=(const ConnectRequest&) = delete;
  ConnectRequest(ConnectRequest&&) noexcept;
  ConnectRequest& operator=(ConnectRequest&&) noexcept;
  ~ConnectRequest();

  // This returns true for a valid and in progress ConnectRequest.
  // MarkComplete is called and this returns false when the request
  // completes.
  explicit operator bool() const { return request_id_; }

  uint64_t request_id() const { return request_id_; }

  // Records that the requested connect operation is complete so it doesn't
  // need to attempt a cancel on destruction.
  void MarkComplete() { request_id_ = 0; }

 private:
  ProtocolConnectionClient* parent_ = nullptr;
  // The `request_id_` of a valid ConnectRequest should be greater than 0.
  uint64_t request_id_ = 0;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_CONNECT_REQUEST_H_
