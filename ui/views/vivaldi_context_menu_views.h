//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Replacement for RenderViewContextMenuViews in chrome. That class will pull
// the entire chain of render_view_context_menu which we do not want because
// of limited support for accelerators and no support for icons.

#ifndef UI_VIEWS_VIVALDI_CONTEXT_MENU_VIEWS_H_
#define UI_VIEWS_VIVALDI_CONTEXT_MENU_VIEWS_H_

#include <memory>

#include "content/public/common/context_menu_params.h"
#include "ui/vivaldi_context_menu.h"

namespace aura {
class Window;
}

namespace content {
class RenderFrameHost;
}

namespace gfx {
class Image;
class Point;
}

namespace ui {
class SimpleMenuModel;
}

namespace views {
class MenuItemView;
class Widget;
}

class ToolkitDelegateViews;

class VivaldiContextMenuViews : public vivaldi::VivaldiContextMenu {
 public:
  ~VivaldiContextMenuViews() override;
  VivaldiContextMenuViews(content::WebContents* web_contents,
                          ui::SimpleMenuModel* menu_model,
                          const content::ContextMenuParams& params);
  void Show() override;
  void SetIcon(const gfx::Image& icon, int id) override;
  void SetSelectedItem(int id) override;
  void UpdateMenu(ui::SimpleMenuModel* menu_model, int id) override;

 private:
  void RunMenuAt(views::Widget* parent,
                 const gfx::Point& point,
                 ui::MenuSourceType type);

 private:
  aura::Window* GetActiveNativeView();
  views::Widget* GetTopLevelWidget();
  std::unique_ptr<ToolkitDelegateViews> toolkit_delegate_;
  content::WebContents* web_contents_;
  ui::SimpleMenuModel* menu_model_;
  content::ContextMenuParams params_;
  views::MenuItemView* menu_view_;  // owned by toolkit_delegate_

  DISALLOW_COPY_AND_ASSIGN(VivaldiContextMenuViews);
};

#endif  // UI_VIEWS_VIVALDI_CONTEXT_MENU_VIEWS_H_
