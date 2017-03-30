// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/bookmarks/bookmarks_private_api.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#if defined(OS_WIN)
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif
#include "chrome/common/extensions/api/bookmarks.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "extensions/schema/bookmarks_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/vivaldi_browser_window.h"


using vivaldi::IsVivaldiApp;
using vivaldi::kVivaldiReservedApiError;
using vivaldi::FindVivaldiBrowser;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace extensions {

namespace bookmarks_private = vivaldi::bookmarks_private;

BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction::
    BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction() {
}

BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction::
    ~BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction() {
}

bool BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction::RunAsync() {
  scoped_ptr<bookmarks_private::UpdateSpeedDialsForWindowsJumplist::Params> params(
      bookmarks_private::UpdateSpeedDialsForWindowsJumplist::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
#if defined(OS_WIN)
  Browser* browser = FindVivaldiBrowser();
  if (browser) {
    if (browser->is_vivaldi()) {
      VivaldiBrowserWindow* browser_view =
          VivaldiBrowserWindow::GetBrowserWindowForBrowser(browser);
      if (browser_view && browser_view->GetJumpList()) {
        JumpList* jump_list = browser_view->GetJumpList();
        if (jump_list)
          jump_list->OnUpdateVivaldiSpeedDials(params->speed_dials);
      }
    } else {
      BrowserView* browser_view =
          BrowserView::GetBrowserViewForBrowser(browser);
      if (browser_view && browser_view->GetJumpList()) {
        JumpList* jump_list = browser_view->GetJumpList();
        if (jump_list)
          jump_list->OnUpdateVivaldiSpeedDials(params->speed_dials);
      }
    }
  }
#endif
  return true;
}

BookmarksPrivateEmptyTrashFunction::BookmarksPrivateEmptyTrashFunction() {
}

BookmarksPrivateEmptyTrashFunction::~BookmarksPrivateEmptyTrashFunction() {
}

bool BookmarksPrivateEmptyTrashFunction::RunOnReady() {
  bool success = false;

  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* trash_node = model->trash_node();
  if (trash_node) {
    while (trash_node->child_count()) {
      const BookmarkNode *remove_node = trash_node->GetChild(0);
      model->Remove(remove_node);
    }
    success = true;
  }
  results_ = vivaldi::bookmarks_private::EmptyTrash::Results::Create(success);
  return true;
}

}  // namespace extensions
