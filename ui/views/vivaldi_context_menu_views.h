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

#include "ui/vivaldi_context_menu.h"

class BookmarkMenuController;

namespace gfx {
class Image;
}

namespace ui {
class SimpleMenuModel;
}

namespace vivaldi {
class ContextMenuPostitionDelegate;
}

namespace views {
class MenuItemView;
class Widget;
}  // namespace views

class ToolkitDelegateViews;

namespace vivaldi {

class VivaldiContextMenuViews : public VivaldiContextMenu {
 public:
  ~VivaldiContextMenuViews() override;
  VivaldiContextMenuViews(
      content::WebContents* web_contents,
      ui::SimpleMenuModel* menu_model,
      const gfx::Rect& rect,
      VivaldiRenderViewContextMenu* render_view_context_menu);
  VivaldiContextMenuViews(const VivaldiContextMenuViews&) = delete;
  VivaldiContextMenuViews& operator=(const VivaldiContextMenuViews&) = delete;

  void Init(ui::SimpleMenuModel* menu_model,
            base::WeakPtr<ContextMenuPostitionDelegate> delegate) override;
  bool Show() override;
  void SetIcon(const gfx::Image& icon, int id) override;
  void SetTitle(const std::u16string& title, int id) override;
  void Refresh() override;
  void UpdateMenu(ui::SimpleMenuModel* menu_model, int id) override;
  bool HasDarkTextColor() override;
  bool IsViews() override { return true; }

 private:
  void RunMenuAt(views::Widget* parent,
                 const gfx::Rect& rect,
                 ui::mojom::MenuSourceType type);

  std::unique_ptr<ToolkitDelegateViews> toolkit_delegate_;
  const raw_ptr<content::WebContents> web_contents_;
  raw_ptr<ui::SimpleMenuModel> menu_model_ = nullptr;
  raw_ptr<views::MenuItemView> menu_view_ = nullptr;  // owned by toolkit_delegate_
  gfx::Rect rect_;
  const raw_ptr<VivaldiRenderViewContextMenu> render_view_context_menu_;
};

}  // namespace vivaldi

#endif  // UI_VIEWS_VIVALDI_CONTEXT_MENU_VIEWS_H_
