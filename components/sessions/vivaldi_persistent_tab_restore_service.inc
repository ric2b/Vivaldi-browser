// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

// included from components/sessions/core/persistent_tab_restore_service.cc

// Inside namespace sessions and anonymous ;

// part of switch case in function
// PersistentTabRestoreService::Delegate::CreateEntriesFromCommands
// Might later be converted to functions, but those will need to update the
// variables of the caller, which is why a macro was used instead
#define VIVALDI_PERSISTENT_TAB_CASES \
  case kCommandSetWindowExtData: {                \
    if (!current_window) {                        \
      /* We should have created a window already.*/ \
      NOTREACHED();                               \
      return;                                     \
    }                                             \
                                                  \
    SessionID::id_type window_id;                 \
    std::string ext_data;                         \
    if (!vivaldi::RestoreSetWindowExtDataCommand( \
      command, &window_id, &ext_data))            \
      return;                                     \
                                                  \
    current_window->ext_data.swap(ext_data);      \
    break;                                        \
  }                                               \
                                                  \
  case kCommandSetExtData: {                      \
    if (!current_tab) {                           \
      /* Should be in a tab when we get this. */  \
      return;                                     \
    }                                             \
    SessionID::id_type tab_id;                    \
    std::string ext_data;                         \
    if (!vivaldi::RestoreSetExtDataCommand(       \
      command, &tab_id,                           \
      &ext_data)) {                               \
      return;                                     \
    }                                             \
    current_tab->ext_data.swap(ext_data);         \
    break;                                        \
  }

void VivaldiTabScheduleExtCommand(BaseSessionService *base_session_service_,
                                  const TabRestoreService::Tab& tab) {
  if (!tab.ext_data.empty()) {
    base_session_service_->ScheduleCommand(
      vivaldi::CreateSetTabExtDataCommand(kCommandSetExtData,
                                 tab.id,
                                 tab.ext_data));
  }
}

void VivaldiWindowsScheduleExtCommand(BaseSessionService *base_session_service_,
                                  const TabRestoreService::Window& window) {
  if (!window.ext_data.empty()) {
    base_session_service_->ScheduleCommand(
      vivaldi::CreateSetWindowExtDataCommand(kCommandSetWindowExtData,
                                      window.id,
                                      window.ext_data));
  }
}

// gisli@vivaldi.com:  Added previous_active_entries.
void PersistentTabRestoreService::Delegate::OnGotPreviousSessionActive(
  std::vector<std::unique_ptr<SessionWindow>> windows,
    SessionID::id_type ignored_active_window) {
  // Keep track of many windows we need to restore, ignore popups.
  previous_active_entries_count_ = 0;
  for (size_t i = 0; i < windows.size(); ++i) {
    if (windows[i]->ext_data.find("\"windowType\":\"popup\"") == std::string::npos) {
      previous_active_entries_count_++;
    }
  }
  // special case: if all the remaining windows where popups, there must have
  // been exactly one active window.
  if (previous_active_entries_count_ == 0 && windows.size() > 0) {
    previous_active_entries_count_ = 1;
  }
}
