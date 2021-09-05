// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef UPDATE_NOTIFIER_UPDATE_NOTIFIER_SWITCHES_H_
#define UPDATE_NOTIFIER_UPDATE_NOTIFIER_SWITCHES_H_

namespace vivaldi_update_notifier {

// Command line parameters for the update notifier.
extern const char kCheckForUpdates[];
extern const char kQuit[];

// Debugging helpers that are not supported in the official build.
extern const char kUpdateExeDir[];
extern const char kUpdateKeepRunning[];
extern const char kUpdateVersion[];

}  // namespace vivaldi_update_notifier

#endif  // UPDATE_NOTIFIER_UPDATE_NOTIFIER_SWITCHES_H_
