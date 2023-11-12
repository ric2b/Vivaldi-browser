//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef BROWSER_MENUS_PWA_MENU_CONTROLLER_H_
#define BROWSER_MENUS_PWA_MENU_CONTROLLER_H_

#include <string>

#include "base/memory/raw_ptr.h"

class Browser;

namespace ui {
class SimpleMenuModel;
}

namespace vivaldi {

// Menu controller that deals with Progressive Web Apps.
class PWAMenuController {
 public:
  PWAMenuController(Browser* browser);

  void PopulateModel(ui::SimpleMenuModel* menu_model);
  bool HandleCommand(int command_id);
  bool IsCommand(int command_id) const;

  // Using the same names as in SimpleMenuModel::Delegate
  bool IsItemForCommandIdDynamic(int command_id) const;
  std::u16string GetLabelForCommandId(int command_id) const;

 private:
  const raw_ptr<Browser> browser_;
};

}  // namespace vivaldi

#endif  //  BROWSER_MENUS_DEVELOPERTOOLS_MENU_CONTROLLER_H_