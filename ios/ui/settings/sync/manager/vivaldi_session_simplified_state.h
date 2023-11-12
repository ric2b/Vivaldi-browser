// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_MANAGER_VIVALDI_SESSION_SIMPLIFIED_STATE_H_
#define IOS_UI_SETTINGS_SYNC_MANAGER_VIVALDI_SESSION_SIMPLIFIED_STATE_H_

// Enum class for the session state. This combines the account, sync,
// and sync settings states and simplifies things for UI.
enum class VivaldiSessionSimplifiedState {
  LOGGED_OUT = 0,
  LOGGING_IN,
  LOGGED_IN_SYNC_OFF,
  LOGGED_IN_SYNC_SUCCESS,
  LOGGED_IN_SYNC_SUCCESS_NO_TABS,
  LOGGED_IN_SYNC_ERROR,
  LOGGED_IN_SYNC_STOPPED,
  NOT_ACTIVATED,
  LOGIN_FAILED,
};

#endif  // IOS_UI_SETTINGS_SYNC_MANAGER_VIVALDI_SESSION_SIMPLIFIED_STATE_H_
