//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef UI_VIEWS_VIVALDI_MENUBAR_MENU_VIEWS_H_
#define UI_VIEWS_VIVALDI_MENUBAR_MENU_VIEWS_H_

#include "ui/vivaldi_context_menu.h"

namespace views {
class MenuItemView;
class Widget;
}

namespace vivaldi {
class Menubar;

class VivaldiMenubarMenuViews : public VivaldiMenubarMenu {
 public:
  ~VivaldiMenubarMenuViews() override;
  VivaldiMenubarMenuViews(content::WebContents* web_contents,
                          MenubarMenuParams& params,
                          int id);
  bool CanShow() override;
  void Show() override;

 private:
  content::WebContents* web_contents_;
  std::unique_ptr<Menubar> menubar_;
  DISALLOW_COPY_AND_ASSIGN(VivaldiMenubarMenuViews);
};

}  // namespace vivialdi


#endif  // UI_VIEWS_VIVALDI_MENUBAR_MENU_VIEWS_H_