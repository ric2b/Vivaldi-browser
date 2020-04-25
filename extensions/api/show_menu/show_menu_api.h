//
// Copyright (c) 2014-2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_SHOW_MENU_SHOW_MENU_API_H_
#define EXTENSIONS_API_SHOW_MENU_SHOW_MENU_API_H_

#include "extensions/browser/extension_function.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {

class ShowMenuAPI {
 public:
  static void SendOpen(Profile* profile);
  static void SendClose(Profile* profile);
  static void SendUrlHighlighted(Profile* profile, const std::string& url);
  static void SendBookmarkActivated(Profile* profile, int id, int event_flags);
  static void SendAddBookmark(Profile* profile, int id);
};

class ShowMenuShowContextMenuFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("showMenu.showContextMenu",
                             SHOWMENU_SHOW_CONTEXT_MENU)
  ShowMenuShowContextMenuFunction() = default;

  void SendResult(int command_id, int event_flags);

 protected:
  ~ShowMenuShowContextMenuFunction() override = default;

  // UIThreadExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShowMenuShowContextMenuFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_SHOW_MENU_SHOW_MENU_API_H_
