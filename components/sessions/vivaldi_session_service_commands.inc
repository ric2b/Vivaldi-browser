// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

// included from components/sessions/core/session_service_commands.cc

// inside namespace sessions;

// part of switch case in function
// PersistentTabRestoreService::Delegate::CreateEntriesFromCommands
// Might later be converted to functions, but those will need to update the
// variables of the caller, which is why a macro was used instead
#define VIVALDI_SESSION_SERVICE_CASES             \
  case kCommandSetExtData:                        \
  case kCommandSetWindowExtData:                  \
  case kCommandPageActionOverrides:               \
    if (!ReadVivaldiData(command, tabs, windows)) \
      return true;                                \
    break;

bool ReadVivaldiData(const SessionCommand* command,
                     IdToSessionTab* tabs,
                     IdToSessionWindow* windows) {
  switch (command->id()) {
    case kCommandSetExtData: {
      SessionID tab_id = SessionID::InvalidValue();
      std::string ext_data;
      if (!vivaldi::RestoreSetExtDataCommand(*command, &tab_id, &ext_data)) {
        return false;
      }

      GetTab(tab_id, tabs)->ext_data.swap(ext_data);
      break;
    }

    case kCommandSetWindowExtData: {
      SessionID window_id = SessionID::InvalidValue();
      std::string ext_data;
      if (!vivaldi::RestoreSetWindowExtDataCommand(*command, &window_id,
                                                   &ext_data))
        return false;

      GetWindow(window_id, windows)->ext_data.swap(ext_data);
      break;
    }

    case kCommandPageActionOverrides: {
      SessionID tab_id = SessionID::InvalidValue();
      std::string script_path;
      bool is_enabled_override;
      if (!vivaldi::RestorePageActionOverrideCommand(
              *command, &tab_id, &script_path, &is_enabled_override)) {
        return false;
      }

      GetTab(tab_id, tabs)
          ->page_action_overrides.emplace(
              std::make_pair(script_path, is_enabled_override));
      break;
    }

    case kCommandRemovePageActionOverrides: {
      SessionID tab_id = SessionID::InvalidValue();
      std::string script_path;
      if (!vivaldi::RestoreRemovePageActionOverrideCommand(*command, &tab_id,
                                                           &script_path)) {
        return false;
      }

      GetTab(tab_id, tabs)->page_action_overrides.erase(script_path);
      break;
    }
  }
  return true;
}
