// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/instance_key.h"

#include <vector>

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "discovery/dnssd/impl/conversion_layer.h"
#include "discovery/dnssd/impl/service_key.h"
#include "discovery/dnssd/public/dns_sd_instance.h"
#include "discovery/mdns/public/mdns_constants.h"
#include "discovery/mdns/public/mdns_records.h"

namespace openscreen::discovery {

InstanceKey::InstanceKey(const MdnsRecord& record)
    : InstanceKey(GetDomainName(record)) {}

InstanceKey::InstanceKey(const DomainName& domain)
    : ServiceKey(domain), instance_id_(domain.labels()[0]) {
  OSP_DCHECK(IsInstanceValid(instance_id_));
}

InstanceKey::InstanceKey(const DnsSdInstance& instance)
    : InstanceKey(instance.instance_id(),
                  instance.service_id(),
                  instance.domain_id()) {}

InstanceKey::InstanceKey(std::string_view instance,
                         std::string_view service,
                         std::string_view domain)
    : ServiceKey(service, domain), instance_id_(instance) {
  OSP_DCHECK(IsInstanceValid(instance_id_))
      << "invalid instance id" << instance;
}

InstanceKey::InstanceKey(const InstanceKey& other) = default;
InstanceKey::InstanceKey(InstanceKey&& other) noexcept = default;

InstanceKey& InstanceKey::operator=(const InstanceKey& rhs) = default;
InstanceKey& InstanceKey::operator=(InstanceKey&& rhs) noexcept = default;

DomainName InstanceKey::GetName() const {
  std::vector<std::string> labels = ServiceKey::GetName().labels();
  labels.insert(labels.begin(), instance_id());
  return DomainName(std::move(labels));
}

}  // namespace openscreen::discovery
