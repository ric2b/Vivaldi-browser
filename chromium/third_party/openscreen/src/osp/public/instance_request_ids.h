// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_INSTANCE_REQUEST_IDS_H_
#define OSP_PUBLIC_INSTANCE_REQUEST_IDS_H_

#include <cstdint>
#include <map>

namespace openscreen::osp {

// Tracks the next available message request ID per instance by its instance
// id. These can only be incremented while an instance is connected but can
// be reset on disconnection.  This is necessary because all APIs that use CBOR
// messages across a QUIC stream share the `request_id` field, which must be
// unique within a pair of instances.
class InstanceRequestIds final {
 public:
  enum class Role {
    kClient,
    kServer,
  };

  explicit InstanceRequestIds(Role role);
  ~InstanceRequestIds();
  InstanceRequestIds(const InstanceRequestIds&) = delete;
  InstanceRequestIds& operator=(const InstanceRequestIds&) = delete;
  InstanceRequestIds(InstanceRequestIds&&) noexcept = delete;
  InstanceRequestIds& operator=(InstanceRequestIds&&) noexcept = delete;

  uint64_t GetNextRequestId(uint64_t instance_id);
  void ResetRequestId(uint64_t instance_id);
  void Reset();

 private:
  const Role role_;
  std::map<uint64_t, uint64_t> request_ids_by_instance_id_;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_INSTANCE_REQUEST_IDS_H_
