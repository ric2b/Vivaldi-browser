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
    ReadVivaldiData(command, tabs, windows);      \
    break;

void ReadVivaldiData(const SessionCommand* command,
                     IdToSessionTab* tabs,
                     IdToSessionWindow* windows) {
  switch (command->id()) {
    case kCommandSetExtData: {
      SessionID tab_id = SessionID::InvalidValue();
      std::string viv_ext_data;
      if (!vivaldi::RestoreSetVivExtDataCommand(*command, &tab_id, &viv_ext_data)) {
        return;
      }

      GetTab(tab_id, tabs)->viv_ext_data.swap(viv_ext_data);
      break;
    }

    case kCommandSetWindowExtData: {
      SessionID window_id = SessionID::InvalidValue();
      std::string viv_ext_data;
      if (!vivaldi::RestoreSetWindowVivExtDataCommand(*command, &window_id,
                                                   &viv_ext_data))
        return;

      GetWindow(window_id, windows)->viv_ext_data.swap(viv_ext_data);
      break;
    }

    case kCommandPageActionOverrides: {
      SessionID tab_id = SessionID::InvalidValue();
      std::string script_path;
      bool is_enabled_override;
      if (!vivaldi::RestoreVivPageActionOverrideCommand(
              *command, &tab_id, &script_path, &is_enabled_override)) {
        return;
      }

      GetTab(tab_id, tabs)
          ->viv_page_action_overrides.emplace(
              std::make_pair(script_path, is_enabled_override));
      break;
    }

    case kCommandRemovePageActionOverrides: {
      SessionID tab_id = SessionID::InvalidValue();
      std::string script_path;
      if (!vivaldi::RestoreRemoveVivPageActionOverrideCommand(*command, &tab_id,
                                                           &script_path)) {
        return;
      }

      GetTab(tab_id, tabs)->viv_page_action_overrides.erase(script_path);
      break;
    }
  }
  return;
}
