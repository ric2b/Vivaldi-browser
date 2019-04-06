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
