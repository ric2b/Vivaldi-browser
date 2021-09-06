// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef UPDATE_NOTIFIER_NATIVE_MENU_H_
#define UPDATE_NOTIFIER_NATIVE_MENU_H_

#include <Windows.h>

#include <string>
#include <vector>

namespace vivaldi_update_notifier {

class NativeMenu {
 public:
  NativeMenu();
  ~NativeMenu();
  NativeMenu(const NativeMenu&) = delete;
  NativeMenu& operator=(const NativeMenu&) = delete;

  void AppendStringMenuItem(const std::wstring& string,
                            UINT state,
                            UINT item_id);
  void AppendSeparator();

  void ShowMenu(int x, int y, HWND hwnd);

  bool displayed() const { return displayed_; }

 private:
  HMENU menu_ = nullptr;

  bool displayed_ = false;

  std::vector<std::wstring> menu_item_strings_;
};
}  // namespace vivaldi_update_notifier

#endif  // UPDATE_NOTIFIER_NATIVE_MENU_H_
