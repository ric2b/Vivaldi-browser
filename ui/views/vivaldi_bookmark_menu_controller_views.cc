#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_views.h"

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/textfield/textfield.h"

// Allows bookmark menus from a generic view
void BookmarkMenuController::RunMenuAt(const views::View* parent,
                                       const gfx::Rect& rect) {
  menu_delegate_->GetBookmarkModel()->AddObserver(this);
  menu_runner_->RunMenuAt(menu_delegate_->parent(), nullptr, rect,
                          views::MENU_ANCHOR_TOPLEFT,
                          ui::MENU_SOURCE_NONE);
}