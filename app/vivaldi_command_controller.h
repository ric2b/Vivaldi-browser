// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef APP_VIVALDI_COMMAND_CONTROLLER_H
#define APP_VIVALDI_COMMAND_CONTROLLER_H

class CommandUpdater;
class Browser;

namespace vivaldi {
void UpdateCommandsForVivaldi(CommandUpdater &);
bool ExecuteVivaldiCommands(Browser *browser, int id);
}

#endif //APP_VIVALDI_COMMAND_CONTROLLER_H
