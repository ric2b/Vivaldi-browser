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
    std::string viv_ext_data;                                              \
    if (!vivaldi::RestoreSetWindowVivExtDataCommand(command, &window_id,   \
                                                 &viv_ext_data))           \
      return;                                                              \
                                                                           \
    current_window->first->viv_ext_data.swap(viv_ext_data);                \
    break;                                                                 \
  }                                                                        \
                                                                           \
  case kCommandSetExtData: {                                               \
    if (!current_tab) {                                                    \
      /* Should be in a tab when we get this. */                           \
      return;                                                              \
    }                                                                      \
    SessionID tab_id = SessionID::InvalidValue();                          \
    std::string viv_ext_data;                                              \
    if (!vivaldi::RestoreSetVivExtDataCommand(command, &tab_id, &viv_ext_data)) { \
      return;                                                              \
    }                                                                      \
    current_tab->viv_ext_data.swap(viv_ext_data);                          \
    break;                                                                 \
  }                                                                        \
                                                                           \
  case kCommandPageActionOverrides: {                                      \
    if (!current_tab)                                                      \
      return;                                                              \
    SessionID tab_id = SessionID::InvalidValue();                          \
    std::string script_path;                                               \
    bool is_enabled_override;                                              \
    if (!vivaldi::RestoreVivPageActionOverrideCommand(                     \
            command, &tab_id, &script_path, &is_enabled_override)) {       \
      return;                                                              \
    }                                                                      \
    current_tab->viv_page_action_overrides.emplace(                        \
        std::make_pair(script_path, is_enabled_override));                 \
    break;                                                                 \
  }

void VivaldiScheduleTabCommands(
    CommandStorageManager* command_storage_manager,
    const tab_restore::Tab& tab) {
  if (!tab.viv_ext_data.empty()) {
    command_storage_manager->ScheduleCommand(
        vivaldi::CreateSetTabVivExtDataCommand(kCommandSetExtData, tab.id,
                                            tab.viv_ext_data));
  }

  for (const auto& viv_page_action_override : tab.viv_page_action_overrides) {
    command_storage_manager->ScheduleCommand(
        vivaldi::CreateVivPageActionOverrideCommand(
            kCommandPageActionOverrides, tab.id, viv_page_action_override.first,
            viv_page_action_override.second));
  }
}

void VivaldiWindowsScheduleExtCommand(
    CommandStorageManager* command_storage_manager,
    const tab_restore::Window& window) {
  if (!window.viv_ext_data.empty()) {
    command_storage_manager->ScheduleCommand(
        vivaldi::CreateSetWindowVivExtDataCommand(kCommandSetWindowExtData,
                                               window.id, window.viv_ext_data));
  }
}

void TabRestoreServiceImpl::VivaldiRequestSave(int num_removed) {
  persistence_delegate_->VivaldiRequestSave(num_removed);
}
