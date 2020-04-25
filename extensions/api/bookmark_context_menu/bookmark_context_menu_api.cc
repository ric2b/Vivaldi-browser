//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#include "extensions/api/bookmark_context_menu/bookmark_context_menu_api.h"

#include "base/strings/utf_string_conversions.h"
#include "browser/menus/vivaldi_menu_enums.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "extensions/api/menubar_menu/menubar_menu_api.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/base/accelerators/accelerator.h"

namespace extensions {

namespace Show = vivaldi::bookmark_context_menu::Show;

using content::BrowserContext;

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    BookmarkContextMenuAPI>>::DestructorAtExit g_bookmark_context_menu =
        LAZY_INSTANCE_INITIALIZER;


BookmarkContextMenuAPI::BookmarkContextMenuAPI(BrowserContext* context) {}

BookmarkContextMenuAPI::~BookmarkContextMenuAPI() {}

// static
BrowserContextKeyedAPIFactory<BookmarkContextMenuAPI>*
    BookmarkContextMenuAPI::GetFactoryInstance() {
  return g_bookmark_context_menu.Pointer();
}

// static
void BookmarkContextMenuAPI::SendOpen(
    content::BrowserContext* browser_context, int64_t id) {
  ::vivaldi::BroadcastEvent(
      vivaldi::bookmark_context_menu::OnOpen::kEventName,
      vivaldi::bookmark_context_menu::OnOpen::Create(std::to_string(id)),
      browser_context);
}

// static
void BookmarkContextMenuAPI::SendClose(
    content::BrowserContext* browser_context) {
  ::vivaldi::BroadcastEvent(vivaldi::bookmark_context_menu::OnClose::kEventName,
                            vivaldi::bookmark_context_menu::OnClose::Create(),
                            browser_context);
}

BookmarkContextMenuShowFunction::BookmarkContextMenuShowFunction() = default;
BookmarkContextMenuShowFunction::~BookmarkContextMenuShowFunction() = default;

ExtensionFunction::ResponseAction BookmarkContextMenuShowFunction::Run() {
  using vivaldi::bookmark_context_menu::Show::Params;
  namespace Results = vivaldi::bookmark_context_menu::Show::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ::vivaldi::BookmarkSorter::SortField sortField;
  switch (params->properties.sort_field) {
    case vivaldi::bookmark_context_menu::SORT_FIELD_NONE:
      sortField = ::vivaldi::BookmarkSorter::FIELD_NONE;
      break;
    case vivaldi::bookmark_context_menu::SORT_FIELD_TITLE:
      sortField = ::vivaldi::BookmarkSorter::FIELD_TITLE;
      break;
    case vivaldi::bookmark_context_menu::SORT_FIELD_URL:
      sortField = ::vivaldi::BookmarkSorter::FIELD_URL;
      break;
    case vivaldi::bookmark_context_menu::SORT_FIELD_NICKNAME:
      sortField = ::vivaldi::BookmarkSorter::FIELD_NICKNAME;
      break;
    case vivaldi::bookmark_context_menu::SORT_FIELD_DESCRIPTION:
      sortField = ::vivaldi::BookmarkSorter::FIELD_NICKNAME;
      break;
    case vivaldi::bookmark_context_menu::SORT_FIELD_DATEADDED:
      sortField = ::vivaldi::BookmarkSorter::FIELD_DATEADDED;
      break;
  }

  ::vivaldi::BookmarkSorter::SortOrder sortOrder;
  switch (params->properties.sort_order) {
    case vivaldi::bookmark_context_menu::SORT_ORDER_NONE:
      sortOrder = ::vivaldi::BookmarkSorter::ORDER_NONE;
      break;
    case vivaldi::bookmark_context_menu::SORT_ORDER_ASCENDING:
      sortOrder = ::vivaldi::BookmarkSorter::ORDER_ASCENDING;
      break;
    case vivaldi::bookmark_context_menu::SORT_ORDER_DESCENDING:
      sortOrder = ::vivaldi::BookmarkSorter::ORDER_DESCENDING;
      break;
  }

  bookmark_menu_container_.reset(new ::vivaldi::BookmarkMenuContainer(this));
  bookmark_menu_container_->support.initIcons(params->properties.icons);
  bookmark_menu_container_->sort_field = sortField;
  bookmark_menu_container_->sort_order = sortOrder;
  bookmark_menu_container_->siblings.reserve(
      params->properties.siblings.size());
  for (const vivaldi::bookmark_context_menu::FolderEntry& e:
       params->properties.siblings) {
    bookmark_menu_container_->siblings.emplace_back();
    ::vivaldi::BookmarkMenuContainerEntry* sibling =
        &bookmark_menu_container_->siblings.back();
    if (!base::StringToInt64(e.id, &sibling->id) || sibling->id <= 0) {
      return RespondNow(Error("id is not a valid bookmark id - " + e.id));
    }
    sibling->offset = e.offset;
    sibling->folder_group = e.folder_group;
    sibling->rect = gfx::Rect(e.rect.x, e.rect.y, e.rect.width, e.rect.height);
  }

  content::WebContents* web_contents = dispatcher()->GetAssociatedWebContents();
  if (web_contents) {
    ::vivaldi::ConvertContainerRectToScreen(web_contents,
        *bookmark_menu_container_);
  }

  std::string error = Open(params->properties.id);
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  AddRef();
  return RespondLater();
}

std::string BookmarkContextMenuShowFunction::Open(const std::string& id) {
  content::WebContents* web_contents = dispatcher()->GetAssociatedWebContents();
  if (!web_contents) {
    return "No WebContents";
  }

  int64_t node_id;
  if (!base::StringToInt64(id, &node_id)) {
    return "id is not a valid int64 - " + id;
  }

  const ::vivaldi::BookmarkMenuContainerEntry* entry = nullptr;
  for (const ::vivaldi::BookmarkMenuContainerEntry& e:
       bookmark_menu_container_->siblings) {
     if (node_id == e.id) {
      entry = &e;
      break;
    }
  }

  if (!entry) {
    return "Unknown menu id";
  }

  bookmarks::BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());

  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(model, node_id);
  if (!node) {
    return "Node with id " + id + "does not exist";
  }

  ::vivaldi::VivaldiBookmarkMenu* menu =
      ::vivaldi::CreateVivaldiBookmarkMenu(web_contents,
          bookmark_menu_container_.get(), node, entry->offset, entry->rect);
  if (!menu->CanShow()) {
    return "Can not show menu";
  }

  BookmarkContextMenuAPI::SendOpen(browser_context(), node_id);
  menu->set_observer(this);
  menu->Show();

  return "";
}

void BookmarkContextMenuShowFunction::OnHover(const std::string& url) {
  MenubarMenuAPI::SendHover(browser_context(), url);
}

void BookmarkContextMenuShowFunction::OnOpenBookmark(int64_t bookmark_id,
                                                     int event_state) {
  MenubarMenuAPI::SendOpenBookmark(browser_context(), bookmark_id, event_state);
}

void BookmarkContextMenuShowFunction::OnBookmarkAction(int64_t bookmark_id,
                                                       int command) {
  MenubarMenuAPI::SendBookmarkAction(browser_context(), bookmark_id, command);
}

void BookmarkContextMenuShowFunction::OnOpenMenu(int64_t bookmark_id) {
   BookmarkContextMenuAPI::SendOpen(browser_context(), bookmark_id);
}

void BookmarkContextMenuShowFunction::BookmarkMenuClosed(
    ::vivaldi::VivaldiBookmarkMenu* menu) {
  namespace Results = vivaldi::bookmark_context_menu::Show::Results;

  BookmarkContextMenuAPI::SendClose(browser_context());
  Respond(ArgumentList(Show::Results::Create()));
  Release();
}

}  // namespace extensions
