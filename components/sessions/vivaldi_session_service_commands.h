// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SESSIONS_VIVALDI_SESSION_SERVICE_COMMANDS_H_
#define COMPONENTS_SESSIONS_VIVALDI_SESSION_SERVICE_COMMANDS_H_

#include <memory>
#include <string>

#include "components/sessions/core/session_id.h"
#include "components/sessions/core/sessions_export.h"

namespace sessions {
class SessionCommand;
}

using sessions::SessionCommand;

namespace vivaldi {

// Creates a SessionCommand that represents ext data.
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetTabExtDataCommand(
    SessionID::id_type command_id,
    SessionID tab_id,
    const std::string& ext_data);

// Creates a SessionCommand stores a browser window's ext data.
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetWindowExtDataCommand(
    SessionID::id_type command_id,
    SessionID window_id,
    const std::string& ext_data);

// Extracts a SessionCommand as previously created by
// CreateSetExtDataCommand into the tab id and ext data.
SESSIONS_EXPORT bool RestoreSetExtDataCommand(const SessionCommand& command,
                                              SessionID* tab_id,
                                              std::string* ext_data);

// Extracts a SessionCommand as previously created by
// CreateSetWindowExtDataCommand into the window id and ext data.
SESSIONS_EXPORT bool RestoreSetWindowExtDataCommand(
    const SessionCommand& command,
    SessionID* window_id,
    std::string* ext_data);

}  // namespace vivaldi

// Functions implemented via incfile for session_service_command.cc,
// have to be defined inside that scope due to constants defined there
namespace sessions {

SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetWindowExtDataCommand(
    const SessionID& window_id,
    const std::string& ext_data);

SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetExtDataCommand(
    const SessionID& tab_id,
    const std::string& ext_data);

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_VIVALDI_SESSION_SERVICE_COMMANDS_H_
