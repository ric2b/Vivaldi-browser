// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef UPDATE_NOTIFIER_UPDATE_NOTIFIER_WINDOW_H_
#define UPDATE_NOTIFIER_UPDATE_NOTIFIER_WINDOW_H_

#include <Windows.h>

#include <memory>
#include <string>

#include "base/win/message_window.h"
#include "update_notifier/native_menu.h"
#include "update_notifier/thirdparty/winsparkle/src/appcast.h"

namespace vivaldi_update_notifier {

class UpdateNotifierWindow {
 public:
  class WindowClass;

  UpdateNotifierWindow();
  ~UpdateNotifierWindow();
  UpdateNotifierWindow(const UpdateNotifierWindow&) = delete;
  UpdateNotifierWindow& operator=(const UpdateNotifierWindow&) = delete;

  void ShowNotification(const std::wstring& version);

 private:
  friend class WindowClass;

  static LRESULT CALLBACK WindowProc(HWND hwnd,
                                     UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam);

  bool HandleMessage(UINT message,
                     WPARAM wparam,
                     LPARAM lparam,
                     LRESULT* result);

  void RemoveNotification();
  bool is_showing_notification_ = false;

  NativeMenu notification_menu_;

  HWND hwnd_;
};

}  // namespace vivaldi_update_notifier

#endif  // UPDATE_NOTIFIER_UPDATE_NOTIFIER_WINDOW_H_
