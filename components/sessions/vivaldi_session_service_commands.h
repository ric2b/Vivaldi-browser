// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SESSIONS_VIVALDI_SESSION_SERVICE_COMMANDS_H_
#define COMPONENTS_SESSIONS_VIVALDI_SESSION_SERVICE_COMMANDS_H_

#include <map>
#include <memory>
#include <string>

#include "base/token.h"
#include "components/page_actions/page_actions_service.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/sessions_export.h"
#include "components/tab_groups/tab_group_id.h"

namespace sessions {
class SessionCommand;
struct SessionTab;
struct SessionWindow;
struct SessionTabGroup;
}  // namespace sessions

using sessions::SessionCommand;

namespace vivaldi {

// Creates a SessionCommand that represents ext data.
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetTabVivExtDataCommand(
    SessionID::id_type command_id,
    SessionID tab_id,
    const std::string& viv_ext_data);

// Creates a SessionCommand stores a browser window's ext data.
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetWindowVivExtDataCommand(
    SessionID::id_type command_id,
    SessionID window_id,
    const std::string& viv_ext_data);

// Creates a SessionCommand that records the override of a page action script
// for a specific tab
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateVivPageActionOverrideCommand(
    SessionID::id_type command_id,
    SessionID tab_id,
    const std::string& script_path,
    bool is_enabled_override);

std::unique_ptr<SessionCommand> CreateVivCreateThumbnailCommand(
    SessionID::id_type command_id,
    int image_format,
    const char * data,
    size_t size);

// Creates a SessionCommand that records the removal of an override of a page
// action script for a specific tab
SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateRemoveVivPageActionOverrideCommand(SessionID::id_type command_id,
                                      SessionID tab_id,
                                      const std::string& script_path);

// Extracts a SessionCommand as previously created by
// CreateSetVivExtDataCommand into the tab id and ext data.
SESSIONS_EXPORT bool RestoreSetVivExtDataCommand(const SessionCommand& command,
                                              SessionID* tab_id,
                                              std::string* viv_ext_data);

// Extracts a SessionCommand as previously created by
// CreateSetWindowExtDataCommand into the window id and ext data.
SESSIONS_EXPORT bool RestoreSetWindowVivExtDataCommand(
    const SessionCommand& command,
    SessionID* window_id,
    std::string* viv_ext_data);

// Extracts a SessionCommand as previously created by
// CreatePageActionOverrideCommand into the tab id and page action script
// override data
SESSIONS_EXPORT bool RestoreVivPageActionOverrideCommand(
    const SessionCommand& command,
    SessionID* tab_id,
    std::string* script_path,
    bool* is_enabled_override);

SESSIONS_EXPORT bool RestoreRemoveVivPageActionOverrideCommand(
    const SessionCommand& command,
    SessionID* tab_id,
    std::string* script_path);

}  // namespace vivaldi

// Functions implemented via incfile for session_service_command.cc,
// have to be defined inside that scope due to constants defined there
namespace sessions {

using IdToSessionTab = std::map<SessionID, std::unique_ptr<SessionTab>>;
using IdToSessionWindow = std::map<SessionID, std::unique_ptr<SessionWindow>>;
using TokenToSessionTabGroup =
    std::map<tab_groups::TabGroupId, std::unique_ptr<SessionTabGroup>>;

SESSIONS_EXPORT uint8_t GetVivCreateThumbnailCommandId();
// The following functions create sequentialized change commands which are
// used to reconstruct the current/previous session state.
// It is up to the caller to delete the returned SessionCommand* object.
SESSIONS_EXPORT void VivaldiCreateTabsAndWindows(
    const std::vector<std::unique_ptr<sessions::SessionCommand>>& data,
    IdToSessionTab* tabs,
    TokenToSessionTabGroup* tab_groups,
    IdToSessionWindow* windows,
    SessionID* active_window_id);

SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateSetSelectedTabInWindowCommand(SessionID window_id, int index);

SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetWindowVivExtDataCommand(
    const SessionID& window_id,
    const std::string& viv_ext_data);

SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetVivExtDataCommand(
    const SessionID& tab_id,
    const std::string& viv_ext_data);

SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateVivPageActionOverrideCommand(
    const SessionID& tab_id,
    const std::string& script_path,
    bool is_enabled_override);

SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateRemoveVivPageActionOverrideCommand(const SessionID& tab_id,
                                      const std::string& script_path);

SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateVivCreateThumbnailCommand(int image_format, const char * data,
    size_t size);

// Filters through the given list of commands - leaving just enough for imported
// tabs to work.
SESSIONS_EXPORT std::vector<std::unique_ptr<sessions::SessionCommand>>
VivaldiFilterImportedTabsSessionCommands(
    std::vector<std::unique_ptr<sessions::SessionCommand>> &cmds,
    bool vivaldi_source);

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_VIVALDI_SESSION_SERVICE_COMMANDS_H_
