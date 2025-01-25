// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "browser/mac/mac_utils.h"

namespace vivaldi {

// Helper for to determine if the NSEvent shall be used to activate a command
// from chrome code. The command is specified in the main menu but in addition
// to mouse activation a command will also be executed on a shortcut match.
// The latter is a problem in Vivaldi because we have our own shortcut
// activation in JS.
// Allow proper mouse and keyboard events. We have observed during rapid
// keypresses that other events are the current event when commandDisptach
// fires (see VB-10837).
bool CanEventExecuteCommand(NSEvent* event) {
  switch ([event type]) {
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp:
    case NSEventTypeLeftMouseDown: // Added for macOs 14.0
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseDown: // Added for macOs 14.0
    case NSEventTypeRightMouseUp:
      return true;
    // VB-23147: All of the remaining events are observed to happen when we
    // execute from Help|Search
    case NSEventTypeMouseMoved:
    case NSEventTypeMouseEntered:
    case NSEventTypeMouseExited:
    case NSEventTypeGesture:
    case NSEventTypeAppKitDefined:
    case NSEventTypeSystemDefined:
      return true;
    default:
      return false;
  }
}

}  // namespace vivaldi