// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

// included from components/sessions/core/session_service_commands.cc

void VivaldiCreateTabsAndWindows(
    const std::vector<std::unique_ptr<sessions::SessionCommand>>& data,
    IdToSessionTab* tabs,
    GroupIdToSessionTabGroup* tab_groups,
    IdToSessionWindow* windows,
    SessionID* active_window_id) {
  // Starting with ch 138 there is holder for discarded windows. This is for
  // window removal in Wayland. We do not use it here, but code inside the
  // call will.
  std::set<SessionID> discarded_window_ids;
  CreateTabsAndWindows(data, tabs, tab_groups, windows, active_window_id,
    nullptr, &discarded_window_ids);
}
