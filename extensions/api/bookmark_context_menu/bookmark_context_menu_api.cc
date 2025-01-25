//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#include "extensions/api/bookmark_context_menu/bookmark_context_menu_api.h"

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "extensions/api/menubar_menu/menubar_menu_api.h"
#include "ui/base/accelerators/accelerator.h"

#include "browser/menus/vivaldi_menu_enums.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/vivaldi_browser_window.h"

namespace extensions {

namespace Show = vivaldi::bookmark_context_menu::Show;

using content::BrowserContext;

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<BookmarkContextMenuAPI>>::DestructorAtExit
    g_bookmark_context_menu = LAZY_INSTANCE_INITIALIZER;

BookmarkContextMenuAPI::BookmarkContextMenuAPI(BrowserContext* context) {}

BookmarkContextMenuAPI::~BookmarkContextMenuAPI() {}

// static
BrowserContextKeyedAPIFactory<BookmarkContextMenuAPI>*
BookmarkContextMenuAPI::GetFactoryInstance() {
  return g_bookmark_context_menu.Pointer();
}

// static
void BookmarkContextMenuAPI::SendOpen(content::BrowserContext* browser_context,
                                      int64_t id) {
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

  params_ = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  VivaldiBrowserWindow* window =
      VivaldiBrowserWindow::FromId(params_->properties.window_id);
  if (!window) {
    return RespondNow(Error("No such window"));
  }

  ::vivaldi::BookmarkSorter::SortField sortField;
  switch (params_->properties.sort_field) {
    case vivaldi::bookmark_context_menu::SortField::kNone:
      sortField = ::vivaldi::BookmarkSorter::FIELD_NONE;
      break;
    case vivaldi::bookmark_context_menu::SortField::kTitle:
      sortField = ::vivaldi::BookmarkSorter::FIELD_TITLE;
      break;
    case vivaldi::bookmark_context_menu::SortField::kUrl:
      sortField = ::vivaldi::BookmarkSorter::FIELD_URL;
      break;
    case vivaldi::bookmark_context_menu::SortField::kNickname:
      sortField = ::vivaldi::BookmarkSorter::FIELD_NICKNAME;
      break;
    case vivaldi::bookmark_context_menu::SortField::kDescription:
      sortField = ::vivaldi::BookmarkSorter::FIELD_NICKNAME;
      break;
    case vivaldi::bookmark_context_menu::SortField::kDateAdded:
      sortField = ::vivaldi::BookmarkSorter::FIELD_DATEADDED;
      break;
  }

  ::vivaldi::BookmarkSorter::SortOrder sortOrder;
  switch (params_->properties.sort_order) {
    case vivaldi::bookmark_context_menu::SortOrder::kNone:
      sortOrder = ::vivaldi::BookmarkSorter::ORDER_NONE;
      break;
    case vivaldi::bookmark_context_menu::SortOrder::kAscending:
      sortOrder = ::vivaldi::BookmarkSorter::ORDER_ASCENDING;
      break;
    case vivaldi::bookmark_context_menu::SortOrder::kDescending:
      sortOrder = ::vivaldi::BookmarkSorter::ORDER_DESCENDING;
      break;
  }

  bookmark_menu_container_.reset(new ::vivaldi::BookmarkMenuContainer(this));
  switch (params_->properties.edge) {
    case vivaldi::bookmark_context_menu::Edge::kAbove:
      bookmark_menu_container_->edge = ::vivaldi::BookmarkMenuContainer::Above;
      break;
    case vivaldi::bookmark_context_menu::Edge::kBelow:
      bookmark_menu_container_->edge = ::vivaldi::BookmarkMenuContainer::Below;
      break;
    default:
      bookmark_menu_container_->edge = ::vivaldi::BookmarkMenuContainer::Off;
      break;
  };
  bookmark_menu_container_->support.initIcons(params_->properties.icons);
  bookmark_menu_container_->sort_field = sortField;
  bookmark_menu_container_->sort_order = sortOrder;
  bookmark_menu_container_->siblings.reserve(
      params_->properties.siblings.size());
  for (const vivaldi::bookmark_context_menu::FolderEntry& e :
       params_->properties.siblings) {
    bookmark_menu_container_->siblings.emplace_back();
    ::vivaldi::BookmarkMenuContainerEntry* sibling =
        &bookmark_menu_container_->siblings.back();
    if (!base::StringToInt64(e.id, &sibling->id) || sibling->id <= 0) {
      return RespondNow(Error("id is not a valid bookmark id - " + e.id));
    }
    sibling->offset = e.offset;
    sibling->folder_group = e.folder_group;
    sibling->rect = gfx::Rect(e.rect.x, e.rect.y, e.rect.width, e.rect.height);
    sibling->menu_index = 0;
    sibling->tweak_separator = false;
  }

  ::vivaldi::ConvertContainerRectToScreen(window->web_contents(),
                                          *bookmark_menu_container_);

  std::string error = Open(window->web_contents(), params_->properties.id);
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  AddRef();
  return RespondLater();
}

std::string BookmarkContextMenuShowFunction::Open(
    content::WebContents* web_contents,
    const std::string& id) {
  int64_t node_id;
  if (!base::StringToInt64(id, &node_id)) {
    return "id is not a valid int64 - " + id;
  }

  const ::vivaldi::BookmarkMenuContainerEntry* entry = nullptr;
  for (const ::vivaldi::BookmarkMenuContainerEntry& e :
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

  ::vivaldi::VivaldiBookmarkMenu* menu = ::vivaldi::CreateVivaldiBookmarkMenu(
      web_contents, bookmark_menu_container_.get(), node, entry->offset,
      entry->rect);
  if (!menu->CanShow()) {
    return "Can not show menu";
  }

  BookmarkContextMenuAPI::SendOpen(browser_context(), node_id);
  menu->set_observer(this);
  menu->Show();

  return "";
}

void BookmarkContextMenuShowFunction::OnHover(const std::string& url) {
  MenubarMenuAPI::SendHover(browser_context(), params_->properties.window_id,
      url);
}

void BookmarkContextMenuShowFunction::OnOpenBookmark(int64_t bookmark_id,
                                                     int event_state) {
  MenubarMenuAPI::SendOpenBookmark(browser_context(),
    params_->properties.window_id, bookmark_id, event_state);
}

void BookmarkContextMenuShowFunction::OnBookmarkAction(int64_t bookmark_id,
                                                       int command) {
  MenubarMenuAPI::SendBookmarkAction(browser_context(),
      params_->properties.window_id, bookmark_id, command);
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
