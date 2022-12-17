// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

// included from components/sessions/core/session_service_commands.cc

// inside namespace sessions;

std::unique_ptr<SessionCommand> CreateSetWindowVivExtDataCommand(
    const SessionID& window_id,
    const std::string& viv_ext_data) {
  return vivaldi::CreateSetWindowVivExtDataCommand(kCommandSetWindowExtData,
                                                window_id, viv_ext_data);
}

std::unique_ptr<SessionCommand> CreateSetVivExtDataCommand(
    const SessionID& tab_id,
    const std::string& viv_ext_data) {
  return vivaldi::CreateSetTabVivExtDataCommand(kCommandSetExtData, tab_id,
                                             viv_ext_data);
}

std::unique_ptr<SessionCommand> CreateVivPageActionOverrideCommand(
    const SessionID& tab_id,
    const std::string& script_path,
    bool is_enabled_override) {
  return vivaldi::CreateVivPageActionOverrideCommand(
      kCommandPageActionOverrides, tab_id, script_path, is_enabled_override);
}

std::unique_ptr<SessionCommand> CreateRemoveVivPageActionOverrideCommand(
    const SessionID& tab_id,
    const std::string& script_path) {
  return vivaldi::CreateRemoveVivPageActionOverrideCommand(
      kCommandRemovePageActionOverrides, tab_id, script_path);
}
