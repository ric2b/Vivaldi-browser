// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_set.h"

#include "components/sync/protocol/product_comparison_specifics.pb.h"

namespace commerce {

ProductSpecificationsSet::ProductSpecificationsSet(
    const std::string& uuid,
    const int64_t creation_time_usec_since_epoch,
    const int64_t update_time_usec_since_epoch,
    const std::vector<GURL>& urls,
    const std::string& name)
    : uuid_(base::Uuid::ParseLowercase(uuid)),
      creation_time_(base::Time::FromMillisecondsSinceUnixEpoch(
          creation_time_usec_since_epoch)),
      update_time_(base::Time::FromMillisecondsSinceUnixEpoch(
          update_time_usec_since_epoch)),
      urls_(urls),
      name_(name) {
  DCHECK(!uuid.empty());
  for (const GURL& url : urls_) {
    DCHECK(url.is_valid());
  }
}

ProductSpecificationsSet::ProductSpecificationsSet(
    const ProductSpecificationsSet&) = default;

ProductSpecificationsSet::~ProductSpecificationsSet() = default;

ProductSpecificationsSet ProductSpecificationsSet::FromProto(
    const sync_pb::ProductComparisonSpecifics& specifics) {
  std::vector<GURL> urls;
  for (const sync_pb::ComparisonData& data : specifics.data()) {
    urls.emplace_back(data.url());
  }
  return ProductSpecificationsSet(
      specifics.uuid(), specifics.creation_time_unix_epoch_millis(),
      specifics.update_time_unix_epoch_millis(), urls, specifics.name());
}

sync_pb::ProductComparisonSpecifics ProductSpecificationsSet::ToProto() const {
  sync_pb::ProductComparisonSpecifics specifics;
  specifics.set_uuid(uuid_.AsLowercaseString());
  specifics.set_name(name_);
  specifics.set_creation_time_unix_epoch_millis(
      creation_time_.InMillisecondsSinceUnixEpoch());
  specifics.set_update_time_unix_epoch_millis(
      update_time_.InMillisecondsSinceUnixEpoch());
  for (const GURL& url : urls_) {
    sync_pb::ComparisonData* data = specifics.add_data();
    data->set_url(url.spec());
  }
  return specifics;
}

}  // namespace commerce
