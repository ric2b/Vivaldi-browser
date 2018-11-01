// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef UI_VIEWS_VIVALDI_PIN_SHORTCUT_H_
#define UI_VIEWS_VIVALDI_PIN_SHORTCUT_H_

namespace extensions {
class AppWindow;
}  // namespace extensions

namespace vivaldi {
void StartPinShortcutToTaskbar(extensions::AppWindow* app_window);
}  // namespace vivaldi

#endif  // UI_VIEWS_VIVALDI_PIN_SHORTCUT_H_
