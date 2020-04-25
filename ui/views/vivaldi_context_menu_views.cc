// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#include "ui/views/vivaldi_context_menu_views.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop_current.h"
#include "chrome/common/chrome_switches.h"
#include "components/renderer_context_menu/views/toolkit_delegate_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/widget/widget.h"

namespace vivaldi {


// Mac uses both a native version and the views version depening menu location
// A separate CreateVivaldiContextMenu exists on the mac implementation.
#if !defined(OS_MACOSX)

VivaldiContextMenu* CreateVivaldiContextMenu(
    content::WebContents* web_contents,
    ui::SimpleMenuModel* menu_model,
    const gfx::Rect& rect,
    bool force_views /* ignored here*/,
    ContextMenuPostitionDelegate* delegate) {
  return new VivaldiContextMenuViews(web_contents, menu_model, rect, delegate);
}

#endif

VivaldiContextMenuViews::VivaldiContextMenuViews(
    content::WebContents* web_contents,
    ui::SimpleMenuModel* menu_model,
    const gfx::Rect& rect,
    ContextMenuPostitionDelegate* delegate)
    : web_contents_(web_contents),
      menu_model_(menu_model),
      rect_(rect) {
  toolkit_delegate_.reset(new ToolkitDelegateViews);
  menu_view_ = toolkit_delegate_->VivaldiInit(menu_model_, delegate);
}

VivaldiContextMenuViews::~VivaldiContextMenuViews() {}

void VivaldiContextMenuViews::RunMenuAt(views::Widget* parent,
                                        const gfx::Rect& rect,
                                        ui::MenuSourceType type) {
  static_cast<ToolkitDelegateViews*>(toolkit_delegate_.get())
      ->VivaldiRunMenuAt(parent, rect, type);
}

void VivaldiContextMenuViews::Show() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return;

  // Menus need a Widget to work. If we're not the active tab we won't
  // necessarily be in a widget.
  views::Widget* widget = GetTopLevelWidgetFromWebContents(web_contents_);
  if (!widget)
    return;

  // Don't show empty menus.
  if (menu_model_->GetItemCount() == 0)
    return;

  // Modify position to become global
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(widget->GetContentsView(), &screen_loc);
  gfx::Point point(rect_.origin());
  point.Offset(screen_loc.x(), screen_loc.y());
  gfx::Rect rect = gfx::Rect(point, rect_.size());

  // Enable recursive tasks on the message loop so we can get updates while
  // the context menu is being displayed.
  base::MessageLoopCurrent::ScopedNestableTaskAllower allow;
  RunMenuAt(widget, rect, ui::MENU_SOURCE_NONE);
}

void VivaldiContextMenuViews::SetIcon(const gfx::Image& icon, int id) {
  if (menu_view_->GetMenuItemByID(id)) {
    menu_view_->SetIcon(*icon.ToImageSkia(), id);
  }
}

void VivaldiContextMenuViews::UpdateMenu(ui::SimpleMenuModel* menu_model,
                                         int id) {
  views::MenuItemView* view = menu_view_->GetMenuItemByID(id);
  if (view)
    toolkit_delegate_->VivaldiUpdateMenu(view, menu_model);
}

}  // namespace vivialdi