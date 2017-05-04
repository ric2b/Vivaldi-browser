// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef UPDATE_NOTIFIER_NATIVE_MENU_H_
#define UPDATE_NOTIFIER_NATIVE_MENU_H_

#include <Windows.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"

namespace vivaldi_update_notifier {

class NativeMenu {
 public:
  NativeMenu();
  ~NativeMenu();

  void Reset();

  bool AppendStringMenuItem(const base::string16& string,
                            UINT state,
                            UINT item_id);
  bool AppendSeparator();

  void ShowMenu(int x, int y, HWND hwnd);

  bool displayed() const { return displayed_; }

 private:
  HMENU menu_;

  bool displayed_;

  std::vector<base::string16> menu_item_strings_;
  DISALLOW_COPY_AND_ASSIGN(NativeMenu);
};
}  // namespace vivaldi_update_notifier

#endif  // UPDATE_NOTIFIER_NATIVE_MENU_H_
