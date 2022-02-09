// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

// included from components/sessions/core/persistent_tab_restore_service.cc

// Inside namespace sessions and anonymous ;

// part of switch case in function
// PersistentTabRestoreService::Delegate::CreateEntriesFromCommands
// Might later be converted to functions, but those will need to update the
// variables of the caller, which is why a macro was used instead
#define VIVALDI_PERSISTENT_TAB_CASES                                       \
  case kCommandSetWindowExtData: {                                         \
    if (!current_window) {                                                 \
      /* We should have created a window already.*/                        \
      NOTREACHED();                                                        \
      return;                                                              \
    }                                                                      \
                                                                           \
    SessionID window_id = SessionID::InvalidValue();                       \
    std::string ext_data;                                                  \
    if (!vivaldi::RestoreSetWindowExtDataCommand(command, &window_id,      \
                                                 &ext_data))               \
      return;                                                              \
                                                                           \
    current_window->first->ext_data.swap(ext_data);                               \
    break;                                                                 \
  }                                                                        \
                                                                           \
  case kCommandSetExtData: {                                               \
    if (!current_tab) {                                                    \
      /* Should be in a tab when we get this. */                           \
      return;                                                              \
    }                                                                      \
    SessionID tab_id = SessionID::InvalidValue();                          \
    std::string ext_data;                                                  \
    if (!vivaldi::RestoreSetExtDataCommand(command, &tab_id, &ext_data)) { \
      return;                                                              \
    }                                                                      \
    current_tab->ext_data.swap(ext_data);                                  \
    break;                                                                 \
  }                                                                        \
                                                                           \
  case kCommandPageActionOverrides: {                                      \
    if (!current_tab)                                                      \
      return;                                                              \
    SessionID tab_id = SessionID::InvalidValue();                          \
    std::string script_path;                                               \
    bool is_enabled_override;                                              \
    if (!vivaldi::RestorePageActionOverrideCommand(                        \
            command, &tab_id, &script_path, &is_enabled_override)) {       \
      return;                                                              \
    }                                                                      \
    current_tab->page_action_overrides.emplace(                            \
        std::make_pair(script_path, is_enabled_override));                 \
    break;                                                                 \
  }

void VivaldiScheduleTabCommands(
    CommandStorageManager* command_storage_manager,
    const TabRestoreService::Tab& tab) {
  if (!tab.ext_data.empty()) {
    command_storage_manager->ScheduleCommand(
        vivaldi::CreateSetTabExtDataCommand(kCommandSetExtData, tab.id,
                                            tab.ext_data));
  }

  for (const auto& page_action_override : tab.page_action_overrides) {
    command_storage_manager->ScheduleCommand(
        vivaldi::CreatePageActionOverrideCommand(
            kCommandPageActionOverrides, tab.id, page_action_override.first,
            page_action_override.second));
  }
}

void VivaldiWindowsScheduleExtCommand(
    CommandStorageManager* command_storage_manager,
    const TabRestoreService::Window& window) {
  if (!window.ext_data.empty()) {
    command_storage_manager->ScheduleCommand(
        vivaldi::CreateSetWindowExtDataCommand(kCommandSetWindowExtData,
                                               window.id, window.ext_data));
  }
}
