// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_WINDOW_WINDOW_PRIVATE_API_H_
#define EXTENSIONS_API_WINDOW_WINDOW_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/schema/window_private.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class Profile;

class VivaldiBrowserWindow;

namespace extensions {

using vivaldi::window_private::WindowState;
WindowState ConvertToJSWindowState(ui::mojom::WindowShowState state);

class VivaldiWindowsAPI {
 public:
  static void Init();

  // Call when all windows for a given profile is being closed.
  static void WindowsForProfileClosing(Profile* profile);

  // Is closing because a profile is closing or not?
  static bool IsWindowClosingBecauseProfileClose(Browser* browser);
};

class WindowPrivateCreateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windowPrivate.create", WINDOW_PRIVATE_CREATE)

  WindowPrivateCreateFunction() = default;

 protected:
  ~WindowPrivateCreateFunction() override = default;

 private:
  ResponseAction Run() override;

  // Fired when the ui-document has loaded. |Window| is now valid.
  void OnAppUILoaded(VivaldiBrowserWindow* window);
};

class WindowPrivateGetCurrentIdFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windowPrivate.getCurrentId",
                             WINDOW_PRIVATE_GET_CURRENT_ID)

  WindowPrivateGetCurrentIdFunction() = default;

 protected:
  ~WindowPrivateGetCurrentIdFunction() override = default;

 private:
  ResponseAction Run() override;
};

class WindowPrivateSetStateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windowPrivate.setState", WINDOW_PRIVATE_SET_STATE)

  WindowPrivateSetStateFunction() = default;

 protected:
  ~WindowPrivateSetStateFunction() override = default;

 private:
  ResponseAction Run() override;
};

class WindowPrivateUpdateMaximizeButtonPositionFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windowPrivate.updateMaximizeButtonPosition",
                             WINDOW_PRIVATE_UPDATE_MAXIMIZE_BUTTON_POSITION)

  WindowPrivateUpdateMaximizeButtonPositionFunction() = default;

 protected:
  ~WindowPrivateUpdateMaximizeButtonPositionFunction() override = default;

 private:
  ResponseAction Run() override;
};

class WindowPrivateGetFocusedElementInfoFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windowPrivate.getFocusedElementInfo",
                             WINDOW_PRIVATE_GET_FOCUSED_ELEMENT_INFO)
  WindowPrivateGetFocusedElementInfoFunction() = default;

 private:
  ~WindowPrivateGetFocusedElementInfoFunction() override = default;
  ResponseAction Run() override;
  void FocusedElementInfoReceived(const std::string& tag_name,
                                  const std::string& type,
                                  bool editable,
                                  const std::string& role);
};

class WindowPrivateIsOnScreenWithNotchFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windowPrivate.isOnScreenWithNotch",
                             WINDOW_PRIVATE_IS_ON_SCREEN_WITH_NOTCH)

  WindowPrivateIsOnScreenWithNotchFunction() = default;

 protected:
  ~WindowPrivateIsOnScreenWithNotchFunction() override = default;

 private:
  ResponseAction Run() override;

  bool IsWindowOnScreenWithNotch(VivaldiBrowserWindow* window);
};

class WindowPrivateSetControlButtonsPaddingFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windowPrivate.setControlButtonsPadding",
                             WINDOW_PRIVATE_SET_CONTROL_BUTTONS_PADDING)

  WindowPrivateSetControlButtonsPaddingFunction() = default;

 protected:
  ~WindowPrivateSetControlButtonsPaddingFunction() override = default;

 private:
  ResponseAction Run() override;

  void RequestChange(gfx::NativeWindow window,
                     vivaldi::window_private::ControlButtonsPadding padding);
};

class WindowPrivatePerformHapticFeedbackFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windowPrivate.performHapticFeedback",
                             WINDOW_PRIVATE_PERFORM_HAPTIC_FEEDBACK)

  WindowPrivatePerformHapticFeedbackFunction() = default;

 protected:
  ~WindowPrivatePerformHapticFeedbackFunction() override = default;

 private:
  ResponseAction Run() override;

  void PerformHapticFeedback();
};

}  // namespace extensions

#endif  // EXTENSIONS_API_WINDOW_WINDOW_PRIVATE_API_H_
