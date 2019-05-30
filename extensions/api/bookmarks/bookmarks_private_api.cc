// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/bookmarks/bookmarks_private_api.h"

#include <memory>
#include <set>
#include <string>

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#if defined(OS_WIN)
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/win/jumplist_factory.h"
#include "chrome/browser/win/jumplist.h"
#endif
#include "chrome/common/extensions/api/bookmarks.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/top_sites.h"
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

VivaldiBookmarksEventRouter::VivaldiBookmarksEventRouter(Profile* profile)
    : browser_context_(profile) {}

VivaldiBookmarksEventRouter::~VivaldiBookmarksEventRouter() {}

void VivaldiBookmarksEventRouter::DispatchEvent(
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->BroadcastEvent(base::WrapUnique(
        new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                              event_name, std::move(event_args))));
  }
}

VivaldiBookmarksAPI::VivaldiBookmarksAPI(content::BrowserContext* context)
    : browser_context_(context), bookmark_model_(nullptr) {
  bookmark_model_ = BookmarkModelFactory::GetForBrowserContext(context);
  DCHECK(bookmark_model_);
  bookmark_model_->AddObserver(this);

  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(
      this, bookmarks_private::OnFaviconChanged::kEventName);
}

VivaldiBookmarksAPI::~VivaldiBookmarksAPI() {}

void VivaldiBookmarksAPI::Shutdown() {
  bookmark_model_->RemoveObserver(this);
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void VivaldiBookmarksAPI::OnListenerAdded(const EventListenerInfo& details) {
  event_router_.reset(new VivaldiBookmarksEventRouter(
      Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<VivaldiBookmarksAPI> >::
    DestructorAtExit g_factory_bookmark = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<VivaldiBookmarksAPI>*
VivaldiBookmarksAPI::GetFactoryInstance() {
  return g_factory_bookmark.Pointer();
}

namespace {
#if 0
void RemoveThumbnailForBookmarkNode(content::BrowserContext* browser_context,
                                    const bookmarks::BookmarkNode* node) {
  scoped_refptr<history::TopSites> top_sites(TopSitesFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context)));
  std::string url = base::StringPrintf("http://bookmark_thumbnail/%d",
                                       static_cast<int>(node->id()));
  top_sites->RemoveThumbnailForUrl(GURL(url));
}
#endif
}  // namespace

void VivaldiBookmarksAPI::BookmarkNodeMoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* old_parent,
    int old_index,
    const bookmarks::BookmarkNode* new_parent,
    int new_index) {
  if (new_parent->type() == BookmarkNode::TRASH) {
    // If it's moved to trash, remove the thumbnail immediately
    const BookmarkNode* node = new_parent->GetChild(new_index);
    DCHECK(node);
#if 0  // Re-enable with new code
    RemoveThumbnailForBookmarkNode(browser_context_, node);
#endif
  }
}

void VivaldiBookmarksAPI::BookmarkNodeRemoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    int old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& no_longer_bookmarked) {
  // We're removing the bookmark (emptying the trash most likely),
  // remove the thumbnail.
#if 0  // Re-enable with new code
  RemoveThumbnailForBookmarkNode(browser_context_, node);
#endif
}

void VivaldiBookmarksAPI::BookmarkMetaInfoChanged(BookmarkModel* model,
                                                  const BookmarkNode* node) {
  bookmarks_private::OnMetaInfoChanged::ChangeInfo change_info;
  change_info.speeddial.reset(new bool(node->GetSpeeddial()));
  change_info.bookmarkbar.reset(new bool(node->GetBookmarkbar()));
  change_info.description.reset(
      new std::string(base::UTF16ToUTF8(node->GetDescription())));
  change_info.thumbnail.reset(
      new std::string(base::UTF16ToUTF8(node->GetThumbnail())));
  change_info.nickname.reset(
      new std::string(base::UTF16ToUTF8(node->GetNickName())));
  // We can add visited time here if we want. Currently not used in UI.

  if (event_router_) {
    event_router_->DispatchEvent(
        bookmarks_private::OnMetaInfoChanged::kEventName,
        bookmarks_private::OnMetaInfoChanged::Create(
            base::Int64ToString(node->id()), change_info));
  }
}

void VivaldiBookmarksAPI::BookmarkNodeFaviconChanged(BookmarkModel* model,
                                                     const BookmarkNode* node) {
  if (event_router_) {
    event_router_->DispatchEvent(
        bookmarks_private::OnFaviconChanged::kEventName,
        bookmarks_private::OnFaviconChanged::Create(
            base::Int64ToString(node->id())));
  }
}


BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction::
    BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction() {}

BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction::
    ~BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction() {}

bool BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction::RunAsync() {
  std::unique_ptr<bookmarks_private::UpdateSpeedDialsForWindowsJumplist::Params>
      params(
          bookmarks_private::UpdateSpeedDialsForWindowsJumplist::Params::Create(
              *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
#if defined(OS_WIN)
  Browser* browser = FindVivaldiBrowser();
  if (browser && browser->is_vivaldi()) {
    JumpList* jump_list = JumpListFactory::GetForProfile(browser->profile());
    if (jump_list)
      jump_list->NotifyVivaldiSpeedDialsChanged(params->speed_dials);
  }
#endif
  SendResponse(true);
  return true;
}

BookmarksPrivateEmptyTrashFunction::BookmarksPrivateEmptyTrashFunction() {}

BookmarksPrivateEmptyTrashFunction::~BookmarksPrivateEmptyTrashFunction() {}

bool BookmarksPrivateEmptyTrashFunction::RunOnReady() {
  bool success = false;

  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* trash_node = model->trash_node();
  if (trash_node) {
    while (trash_node->child_count()) {
      const BookmarkNode* remove_node = trash_node->GetChild(0);
      model->Remove(remove_node);
    }
    success = true;
  }
  results_ = vivaldi::bookmarks_private::EmptyTrash::Results::Create(success);
  return true;
}

}  // namespace extensions
