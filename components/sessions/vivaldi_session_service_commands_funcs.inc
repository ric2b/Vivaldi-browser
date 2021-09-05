// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

// included from components/sessions/core/session_service_commands.cc

// inside namespace sessions;

std::unique_ptr<SessionCommand> CreateSetWindowExtDataCommand(
    const SessionID& window_id,
    const std::string& ext_data) {
  return vivaldi::CreateSetWindowExtDataCommand(kCommandSetWindowExtData,
                                                window_id, ext_data);
}

std::unique_ptr<SessionCommand> CreateSetExtDataCommand(
    const SessionID& tab_id,
    const std::string& ext_data) {
  return vivaldi::CreateSetTabExtDataCommand(kCommandSetExtData, tab_id,
                                             ext_data);
}

std::unique_ptr<SessionCommand> CreatePageActionOverrideCommand(
    const SessionID& tab_id,
    const std::string& script_path,
    bool is_enabled_override) {
  return vivaldi::CreatePageActionOverrideCommand(
      kCommandPageActionOverrides, tab_id, script_path, is_enabled_override);
}

std::unique_ptr<SessionCommand> CreateRemovePageActionOverrideCommand(
    const SessionID& tab_id,
    const std::string& script_path) {
  return vivaldi::CreateRemovePageActionOverrideCommand(
      kCommandRemovePageActionOverrides, tab_id, script_path);
}
