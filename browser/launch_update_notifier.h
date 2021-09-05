// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_LAUNCH_UPDATE_NOTIFIER_H_
#define BROWSER_LAUNCH_UPDATE_NOTIFIER_H_

#include "base/command_line.h"

class Profile;

namespace vivaldi {

void LaunchUpdateNotifier(Profile* profile);

#if defined(OS_WIN)
// Get the command line to start the update notifier with common switches
// reflecting the current settings.
base::CommandLine GetCommonUpdateNotifierCommand(Profile* profile);
#endif

}  // namespace vivaldi

#endif  // BROWSER_LAUNCH_UPDATE_NOTIFIER_H_
