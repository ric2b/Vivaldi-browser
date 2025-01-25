// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef APP_VIVALDI_COMMAND_CONTROLLER_H_
#define APP_VIVALDI_COMMAND_CONTROLLER_H_

class CommandUpdater;
class Browser;

namespace vivaldi {
enum VivaldiScrollType {
  kVivaldiNoScrollType = 0,
  kVivaldiScrollWheel,
  kVivaldiScrollTrackpad,
  kVivaldiScrollInertial
};

bool GetIsEnabledWithNoWindows(int action, bool* enabled);
bool GetIsEnabled(int action, bool hasWindow, bool* enabled);
bool GetIsSupportedInSettings(int action);
bool HasActiveWindow();
void SetVivaldiScrollType(int scrollType);
void UpdateCommandsForVivaldi(CommandUpdater*);
bool ExecuteVivaldiCommands(Browser* browser, int id);
}

#endif  // APP_VIVALDI_COMMAND_CONTROLLER_H_
