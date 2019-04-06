//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//
#ifndef UI_VIVALDI_CONTEXT_MENU_H_
#define UI_VIVALDI_CONTEXT_MENU_H_

namespace content {
class WebContents;
struct ContextMenuParams;
}

namespace gfx {
class Image;
}

namespace ui {
class SimpleMenuModel;
}

namespace vivaldi {
class VivaldiContextMenu;
}

namespace vivaldi {
VivaldiContextMenu* CreateVivaldiContextMenu(
    content::WebContents* web_contents,
    ui::SimpleMenuModel* menu_model,
    const content::ContextMenuParams& params);

class VivaldiContextMenu {
 public:
  virtual ~VivaldiContextMenu() {}

  virtual void Show() = 0;
  virtual void SetIcon(const gfx::Image& icon, int id) {}
  virtual void SetSelectedItem(int id) {}
  virtual void UpdateMenu(ui::SimpleMenuModel* menu_model, int id) {}
};

}  // namespace vivaldi

#endif  // UI_VIVALDI_CONTEXT_MENU_H_
