// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_WINDOW_WINDOW_PRIVATE_API_H_
#define EXTENSIONS_API_WINDOW_WINDOW_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/schema/window_private.h"

class Browser;
class Profile;

class VivaldiBrowserWindow;

namespace extensions {

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

}  // namespace extensions

#endif  // EXTENSIONS_API_WINDOW_WINDOW_PRIVATE_API_H_
