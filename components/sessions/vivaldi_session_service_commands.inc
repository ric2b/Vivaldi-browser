// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

// included from components/sessions/core/session_service_commands.cc

// inside namespace sessions;

// part of switch case in function
// PersistentTabRestoreService::Delegate::CreateEntriesFromCommands
// Might later be converted to functions, but those will need to update the
// variables of the caller, which is why a macro was used instead
#define VIVALDI_SESSION_SERVICE_CASES                \
  case kCommandSetExtData:                           \
  case kCommandSetWindowExtData:                     \
    if (!VivaldiReadExtdata(command, tabs, windows)) \
      return true;                                   \
    break;

bool VivaldiReadExtdata(const SessionCommand* command,
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
  }
  return true;
}
