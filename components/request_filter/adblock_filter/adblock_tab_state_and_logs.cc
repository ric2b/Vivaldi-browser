// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_tab_state_and_logs.h"

namespace adblock_filter {

TabStateAndLogs::BlockedTrackerInfo::BlockedTrackerInfo() = default;
TabStateAndLogs::BlockedTrackerInfo::~BlockedTrackerInfo() = default;
TabStateAndLogs::BlockedTrackerInfo::BlockedTrackerInfo(
    BlockedTrackerInfo&& other) = default;

TabStateAndLogs::TabBlockedUrlInfo::TabBlockedUrlInfo() = default;
TabStateAndLogs::TabBlockedUrlInfo::~TabBlockedUrlInfo() = default;
TabStateAndLogs::TabBlockedUrlInfo::TabBlockedUrlInfo(
    TabBlockedUrlInfo&& other) = default;
TabStateAndLogs::TabBlockedUrlInfo&
TabStateAndLogs::TabBlockedUrlInfo::operator=(TabBlockedUrlInfo&& other) =
    default;

TabStateAndLogs::~TabStateAndLogs() = default;
}  // namespace adblock_filter
