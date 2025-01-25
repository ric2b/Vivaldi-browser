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

std::unique_ptr<SessionCommand> CreateVivCreateThumbnailCommand(int image_format,
    const char * data,  size_t size) {
  return vivaldi::CreateVivCreateThumbnailCommand(kCommandCreateThumbnail,
    image_format, data, size);
}

uint8_t GetVivCreateThumbnailCommandId() {
  return kCommandCreateThumbnail;
}

/// This is used by open tab import.
std::vector<std::unique_ptr<sessions::SessionCommand>>
VivaldiFilterImportedTabsSessionCommands(
    std::vector<std::unique_ptr<sessions::SessionCommand>> &cmds,
    bool vivaldi_source) {
  static const SessionCommand::id_type kChromeCommandSetTabGroup = 25;

  std::vector<std::unique_ptr<sessions::SessionCommand>> commands;
  for (auto &item : cmds) {
    // we use a filter for the commands that we want to pass through - many are
    // irrelevant/not parseable and would stop the creation operation
    // prematurely.
    switch (item->id()) {
      case kCommandSetTabIndexInWindow:
      case kCommandUpdateTabNavigation:
      case kCommandSetSelectedNavigationIndex:
      case kCommandSetSelectedTabInIndex:
      case kCommandSetPinnedState:
      case kCommandSetTabUserAgentOverride:
      case kCommandSessionStorageAssociated:
      case kCommandTabClosed:
        commands.push_back(std::move(item));
        break;
      case kChromeCommandSetTabGroup:
        // Vivaldi has a different command id for SetTabGroup operation than
        // rest of the chromium browsers.
        if (!vivaldi_source) {
          auto adapted_command =
              std::make_unique<sessions::SessionCommand>(kCommandSetTabGroup, item->size());
          item->GetPayload(adapted_command->contents(), item->size());
          commands.push_back(std::move(adapted_command));
        }
        break;
      // Commands below are all vivaldi specific:
      case kCommandSetExtData:
      case kCommandSetTabGroup:
      case kCommandPageActionOverrides:
      case kCommandCreateThumbnail:
      case kCommandRemovePageActionOverrides:
        if (vivaldi_source) {
          commands.push_back(std::move(item));
        }
        break;
      default:
        continue;
    };
  }

  return commands;
}
