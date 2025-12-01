// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/adblock_stats_data.h"

namespace adblock_filter {

StatsData::StatsData() = default;
StatsData::~StatsData() = default;

StatsData::StatsData(StatsData&& other) = default;
StatsData& StatsData::operator=(StatsData&& other) = default;

void StatsData::AddEntry(const Entry& entry, EntryType type) {
  switch (type) {
    case EntryType::WEBSITE:
      website_entries_.push_back(entry);
      break;
    case EntryType::TRACKER_AND_ADS:
      // Websites and tracker and ads are tracking same events, but pointing to
      // a different host (actual tracker or origin), thus we increment the
      // counters only for trackers and ads.
      total_ads_blocked_ += entry.ad_count;
      total_trackers_blocked_ += entry.tracker_count;
      tracker_entries_.push_back(entry);
      break;
  }
}

void StatsData::SetReportingStart(base::Time timestamp) {
  reporting_start_timestamp_ = timestamp;
}

}  // namespace adblock_filter
