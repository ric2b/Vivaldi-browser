//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef BROWSER_MENUS_DEVELOPERTOOLS_MENU_CONTROLLER_H_
#define BROWSER_MENUS_DEVELOPERTOOLS_MENU_CONTROLLER_H_

#include "ui/gfx/geometry/point.h"

class Browser;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace ui {
class Accelerator;
class SimpleMenuModel;
}

namespace vivaldi {

class DeveloperToolsMenuController {
 public:
  DeveloperToolsMenuController(content::WebContents* web_contents,
                               const gfx::Point& location);

  void PopulateModel(ui::SimpleMenuModel* menu_model);
  bool HandleCommand(int command_id);
  bool IsCommand(int command_id) const;

  // Using the same names as in SimpleMenuModel::Delegate
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const;


 private:
  bool HasFeature();
  const extensions::Extension* GetExtension() const;

  content::WebContents* web_contents_;
  Browser* browser_;
  gfx::Point location_;
  bool enabled_;
};

}  // vivaldi

#endif  //  BROWSER_MENUS_DEVELOPERTOOLS_MENU_CONTROLLER_H_