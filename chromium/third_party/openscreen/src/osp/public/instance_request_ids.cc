// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/instance_request_ids.h"

namespace openscreen::osp {

InstanceRequestIds::InstanceRequestIds(Role role) : role_(role) {}
InstanceRequestIds::~InstanceRequestIds() = default;

uint64_t InstanceRequestIds::GetNextRequestId(uint64_t instance_id) {
  uint64_t& next_request_id = request_ids_by_instance_id_[instance_id];
  uint64_t request_id = next_request_id + (role_ == Role::kServer);
  next_request_id += 2;
  return request_id;
}

void InstanceRequestIds::ResetRequestId(uint64_t instance_id) {
  request_ids_by_instance_id_.erase(instance_id);
}

void InstanceRequestIds::Reset() {
  request_ids_by_instance_id_.clear();
}

}  // namespace openscreen::osp
