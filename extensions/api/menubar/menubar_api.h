//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_MENUBAR_API_H_
#define EXTENSIONS_API_MENUBAR_API_H_

#include "extensions/browser/extension_function.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class MenubarAPI {
 public:
  static void SendOnActivated(content::BrowserContext* browser_context,
                              int window_id,
                              int command_id,
                              const std::string& parameter = "");
};

class MenubarSetupFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("menubar.setup", MENUBAR_SETUP)
  MenubarSetupFunction() = default;

 protected:
  ~MenubarSetupFunction() override = default;

  // UIThreadExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MenubarSetupFunction);
};

}  // namespace extensions


#endif  // EXTENSIONS_API_MENUBAR_API_H_