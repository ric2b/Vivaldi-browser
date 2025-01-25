//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_MENUBAR_API_H_
#define EXTENSIONS_API_MENUBAR_API_H_

#include "extensions/browser/extension_function.h"

namespace content {
class BrowserContext;
}

class CommandUpdater;

namespace extensions {

class MenubarAPI {
 public:
  static void UpdateCommandEnabled(CommandUpdater* command_updater);
  static bool GetIsEnabledWithNoWindows(int id, bool* enabled);
  static bool GetIsEnabled(int id, bool hasWindow, bool* enabled);
  static bool GetIsSupportedInSettings(int id);
  static bool HasActiveWindow();
  static bool HandleActionById(content::BrowserContext* browser_context,
                               int window_id,
                               int command_id,
                               const std::string& parameter = "");
};


class MenubarGetHasWindowsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("menubar.getHasWindows", MENUBAR_GET_HAS_WINDOWS)
  MenubarGetHasWindowsFunction() = default;

 private:
  ~MenubarGetHasWindowsFunction() override = default;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;
};

class MenubarSetupFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("menubar.setup", MENUBAR_SETUP)
  MenubarSetupFunction() = default;

 private:
  ~MenubarSetupFunction() override = default;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_MENUBAR_API_H_