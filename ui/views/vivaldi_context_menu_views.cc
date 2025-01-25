// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#include "ui/views/vivaldi_context_menu_views.h"

#include "base/command_line.h"
#include "base/task/current_thread.h"
#include "browser/menus/vivaldi_render_view_context_menu.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "components/renderer_context_menu/views/toolkit_delegate_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/widget.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

// Mac uses both a native version and the views version depening menu location
// A separate CreateVivaldiContextMenu exists on the mac implementation.
#if !BUILDFLAG(IS_MAC)

VivaldiContextMenu* CreateVivaldiContextMenu(
    content::WebContents* web_contents,
    ui::SimpleMenuModel* menu_model,
    const gfx::Rect& rect,
    bool force_views /* ignored here*/,
    VivaldiRenderViewContextMenu* render_view_context_menu) {
  return new VivaldiContextMenuViews(web_contents, menu_model, rect,
                                     render_view_context_menu);
}

#endif

VivaldiContextMenuViews::VivaldiContextMenuViews(
    content::WebContents* web_contents,
    ui::SimpleMenuModel* menu_model,
    const gfx::Rect& rect,
    VivaldiRenderViewContextMenu* render_view_context_menu)
    : web_contents_(web_contents),
      menu_model_(menu_model),
      rect_(rect),
      render_view_context_menu_(render_view_context_menu) {
  if (render_view_context_menu_) {
    std::unique_ptr<RenderViewContextMenu::ToolkitDelegate> delegate(
        new ToolkitDelegateViews);
    render_view_context_menu->set_toolkit_delegate(std::move(delegate));
  }
  Profile* profile = Profile::FromBrowserContext(
      web_contents->GetBrowserContext());
  views::MenuController::VivaldiSetCompactLayout(
      profile->GetPrefs()->GetBoolean(vivaldiprefs::kMenuCompact));
  views::MenuController::VivaldiSetContextMenu(true);
}

VivaldiContextMenuViews::~VivaldiContextMenuViews() {}

void VivaldiContextMenuViews::Init(
    ui::SimpleMenuModel* menu_model,
    base::WeakPtr<ContextMenuPostitionDelegate> delegate) {
  menu_model_ = menu_model;
  if (render_view_context_menu_) {
    menu_view_ =
        static_cast<ToolkitDelegateViews*>(
            render_view_context_menu_->toolkit_delegate())
                ->VivaldiInit(menu_model_, delegate);
  } else {
    std::unique_ptr<ToolkitDelegateViews> toolkitdelegate(
        new ToolkitDelegateViews);
    toolkit_delegate_ = std::move(toolkitdelegate);
    menu_view_ = toolkit_delegate_->VivaldiInit(menu_model_, delegate);
  }
}

void VivaldiContextMenuViews::RunMenuAt(views::Widget* parent,
                                        const gfx::Rect& rect,
                                        ui::mojom::MenuSourceType type) {
  if (render_view_context_menu_) {
    static_cast<ToolkitDelegateViews*>(
        render_view_context_menu_->toolkit_delegate())
            ->VivaldiRunMenuAt(parent, rect, type);
  } else {
    toolkit_delegate_->VivaldiRunMenuAt(parent, rect, type);
  }
}

bool VivaldiContextMenuViews::Show() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return false;

  // Menus need a Widget to work. If we're not the active tab we won't
  // necessarily be in a widget.
  views::Widget* widget = GetTopLevelWidgetFromWebContents(web_contents_);
  if (!widget)
    return false;

  // Don't show empty menus.
  if (menu_model_->GetItemCount() == 0)
    return false;

  // Ensure there are no other active menu. If we call RunMenuAt() below when
  // that is tne case the new menu request will fail silently. That will cause a
  // lockup in JS because it will never receive a message that says the menu
  // has closed.
  // An example. Open the main vivaldi menu and next rightclick in the document
  // when in load-and-launch mode.
  // We can force close the existing menu with
  // controller->Cancel(views::MenuController::ExitType::kAll);
  views::MenuController* controller =
      views::MenuController::GetActiveInstance();
  if (controller)
    return false;

  // Modify position to become global
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(widget->GetContentsView(), &screen_loc);
  gfx::Point point(rect_.origin());
  point.Offset(screen_loc.x(), screen_loc.y());
  gfx::Rect rect = gfx::Rect(point, rect_.size());

  // Enable recursive tasks on the message loop so we can get updates while
  // the context menu is being displayed.
  base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
  RunMenuAt(widget, rect, ui::mojom::MenuSourceType::kNone);
  return true;
}

void VivaldiContextMenuViews::SetIcon(const gfx::Image& icon, int id) {
  if (menu_view_->GetMenuItemByID(id)) {
    menu_view_->GetMenuItemByID(id)->SetIcon(ui::ImageModel::FromImage(icon));
  }
}

void VivaldiContextMenuViews::SetTitle(const std::u16string& title, int id) {
if (menu_view_->GetMenuItemByID(id)) {
    menu_view_->GetMenuItemByID(id)->SetTitle(title);
  }
}

void RefreshInternal(views::SubmenuView* submenu) {
  for (views::MenuItemView* item : submenu->GetMenuItems()) {
    if (item->GetType() == views::MenuItemView::Type::kCheckbox) {
      item->UpdateCheckBoxVisibility();
    }
    if (item->GetType() == views::MenuItemView::Type::kSubMenu ||
        item->GetType() == views::MenuItemView::Type::kActionableSubMenu) {
      if (item->GetSubmenu()) {
        RefreshInternal(item->GetSubmenu());
      }
    }
  }
}

void VivaldiContextMenuViews::Refresh() {
  if (menu_view_->GetSubmenu()) {
    RefreshInternal(menu_view_->GetSubmenu());
  }
}

void VivaldiContextMenuViews::UpdateMenu(ui::SimpleMenuModel* menu_model,
                                         int id) {
  views::MenuItemView* view = menu_view_->GetMenuItemByID(id);
  if (view) {
    if (render_view_context_menu_) {
      static_cast<ToolkitDelegateViews*>(
          render_view_context_menu_->toolkit_delegate())
              ->VivaldiUpdateMenu(view, menu_model);
    } else {
      toolkit_delegate_->VivaldiUpdateMenu(view, menu_model);
    }
  }
}

bool VivaldiContextMenuViews::HasDarkTextColor() {
  views::Widget* widget = GetTopLevelWidgetFromWebContents(web_contents_);
  if (widget && widget->GetColorProvider()) {
    return color_utils::IsDark(
        widget->GetColorProvider()->GetColor(ui::kColorMenuItemForeground));
  } else {
    return true;  // Assume light background
  }
}

}  // namespace vivaldi