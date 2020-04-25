//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef UI_VIEWS_VIVALDI_MENUBAR_MENU_VIEWS_H_
#define UI_VIEWS_VIVALDI_MENUBAR_MENU_VIEWS_H_

#include "ui/vivaldi_context_menu.h"

namespace vivaldi {
class Menubar;
}

namespace aura {
class Window;
}

namespace views {
class MenuItemView;
class Widget;
}

class VivaldiMenubarMenuViews : public vivaldi::VivaldiMenubarMenu {
 public:
  ~VivaldiMenubarMenuViews() override;
  VivaldiMenubarMenuViews(content::WebContents* web_contents,
                          vivaldi::MenubarMenuParams& params,
                          int id);
  bool CanShow() override;
  void Show() override;

  static gfx::NativeView GetActiveNativeViewFromWebContents(
      content::WebContents* web_contents);

 private:
  views::Widget* GetTopLevelWidget();
  Browser* GetBrowser();
  content::WebContents* web_contents_;
  std::unique_ptr<vivaldi::Menubar> menubar_;
  DISALLOW_COPY_AND_ASSIGN(VivaldiMenubarMenuViews);
};

#endif  // UI_VIEWS_VIVALDI_MENUBAR_MENU_VIEWS_H_