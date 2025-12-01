// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_STATS_DATA_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_STATS_DATA_H_

#include <string>
#include <vector>
#include "base/time/time.h"

namespace adblock_filter {

class StatsData {
 public:
  // Tracked types for Privacy Dashboard stats.
  enum class EntryType {
    WEBSITE,
    TRACKER_AND_ADS,
  };
  struct Entry {
    std::string host;
    int64_t ad_count{0};
    int64_t tracker_count{0};

    bool operator<(const Entry& other) const {
      return (tracker_count + ad_count) <
                 (other.tracker_count + other.ad_count) &&
             host < other.host;
    }
  };
  using Entries = std::vector<Entry>;

  StatsData();
  ~StatsData();

  StatsData(StatsData&& other);
  StatsData& operator=(StatsData&& other);

  StatsData(const StatsData&) = delete;
  StatsData& operator=(const StatsData&) = delete;

  void AddEntry(const Entry& entry, EntryType type);
  void SetReportingStart(base::Time timestamp);

  base::Time ReportingStart() const { return reporting_start_timestamp_; }
  const Entries& WebsiteEntries() const { return website_entries_; }
  const Entries& TrackerEntries() const { return tracker_entries_; }

  int64_t TotalAdsBlocked() const { return total_ads_blocked_; }
  int64_t TotalTrackersBlocked() const { return total_trackers_blocked_; }

 private:
  base::Time reporting_start_timestamp_;
  Entries website_entries_;
  Entries tracker_entries_;
  int64_t total_ads_blocked_{0};
  int64_t total_trackers_blocked_{0};
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_STATS_DATA_H_
