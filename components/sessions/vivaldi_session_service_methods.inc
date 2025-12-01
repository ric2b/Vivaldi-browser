// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

// included from components/sessions/core/session_service_commands.cc

void VivaldiCreateTabsAndWindows(
    const std::vector<std::unique_ptr<sessions::SessionCommand>>& data,
    IdToSessionTab* tabs,
    GroupIdToSessionTabGroup* tab_groups,
    IdToSessionWindow* windows,
    SessionID* active_window_id) {
  CreateTabsAndWindows(data, tabs, tab_groups, windows, active_window_id, nullptr);
}
