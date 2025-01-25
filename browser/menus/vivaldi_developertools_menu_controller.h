//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef BROWSER_MENUS_DEVELOPERTOOLS_MENU_CONTROLLER_H_
#define BROWSER_MENUS_DEVELOPERTOOLS_MENU_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
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
}  // namespace ui

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

  void SetHandleInspectElement(bool handle_inspect_element) {
    handle_inspect_element_ = handle_inspect_element;
  }

 private:
  bool HasFeature();
  const extensions::Extension* GetExtension() const;

  const raw_ptr<content::WebContents> web_contents_;
  const raw_ptr<Browser> browser_;
  gfx::Point location_;
  bool enabled_;
  // Tweak for the vivadi render view menu. We let this controller set up the
  // menu element, but defer handling the action to chrome code which serves as
  // fallback for that menu. Chrome code holds proper coordinates for the
  // element to inspect.
  bool handle_inspect_element_ = true;
};

}  // namespace vivaldi

#endif  //  BROWSER_MENUS_DEVELOPERTOOLS_MENU_CONTROLLER_H_