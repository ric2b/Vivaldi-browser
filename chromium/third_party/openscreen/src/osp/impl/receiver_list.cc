// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/receiver_list.h"

#include <algorithm>

namespace openscreen::osp {

ReceiverList::ReceiverList() = default;
ReceiverList::~ReceiverList() = default;

void ReceiverList::OnReceiverAdded(const ServiceInfo& info) {
  receivers_.emplace_back(info);
}

Error ReceiverList::OnReceiverChanged(const ServiceInfo& info) {
  auto existing_info = std::find_if(
      receivers_.begin(), receivers_.end(), [&info](const ServiceInfo& x) {
        return x.instance_name == info.instance_name;
      });
  if (existing_info == receivers_.end())
    return Error::Code::kItemNotFound;

  *existing_info = info;
  return Error::None();
}

ErrorOr<ServiceInfo> ReceiverList::OnReceiverRemoved(const ServiceInfo& info) {
  // All of the removed service infos should be equivalent, so just return the
  // first one.
  ServiceInfo out = info;
  const auto it = std::remove(receivers_.begin(), receivers_.end(), info);
  if (it == receivers_.end())
    return Error::Code::kItemNotFound;

  receivers_.erase(it, receivers_.end());
  return out;
}

Error ReceiverList::OnAllReceiversRemoved() {
  const auto empty = receivers_.empty();
  receivers_.clear();
  return empty ? Error::Code::kItemNotFound : Error::None();
}

}  // namespace openscreen::osp
