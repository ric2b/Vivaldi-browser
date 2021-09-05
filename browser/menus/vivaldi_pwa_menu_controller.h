//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef BROWSER_MENUS_PWA_MENU_CONTROLLER_H_
#define BROWSER_MENUS_PWA_MENU_CONTROLLER_H_

#include "base/strings/string16.h"

class Browser;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace ui {
class SimpleMenuModel;
}

namespace vivaldi {

// Menu controller that deals with Progressive Web Apps.
class PWAMenuController {
 public:
  PWAMenuController(content::WebContents* web_contents);

  void PopulateModel(ui::SimpleMenuModel* menu_model);
  bool HandleCommand(int command_id);
  bool IsCommand(int command_id) const;

  // Using the same names as in SimpleMenuModel::Delegate
  bool IsItemForCommandIdDynamic(int command_id) const;
  base::string16 GetLabelForCommandId(int command_id) const;



 private:
  bool HasFeature();
  const extensions::Extension* GetExtension() const;

  content::WebContents* web_contents_;
  Browser* browser_;
  bool enabled_;
};

}  // vivaldi

#endif  //  BROWSER_MENUS_DEVELOPERTOOLS_MENU_CONTROLLER_H_