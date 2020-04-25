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
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/base/accelerators/accelerator.h"

namespace extensions {

namespace Show = vivaldi::bookmark_context_menu::Show;

using content::BrowserContext;

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    BookmarkContextMenuAPI>>::DestructorAtExit g_bookmark_context_menu =
        LAZY_INSTANCE_INITIALIZER;

// Helper
static vivaldi::bookmark_context_menu::Disposition
    CommandToDisposition(int command) {
  switch(command) {
    case IDC_VIV_BOOKMARK_BAR_OPEN_CURRENT_TAB:
      return vivaldi::bookmark_context_menu::DISPOSITION_CURRENT;
    case IDC_VIV_BOOKMARK_BAR_OPEN_NEW_TAB:
    case IDC_VIV_BOOKMARK_BAR_OPEN_BACKGROUND_TAB:
      return vivaldi::bookmark_context_menu::DISPOSITION_NEW_TAB;
    case IDC_VIV_BOOKMARK_BAR_OPEN_NEW_WINDOW:
      return vivaldi::bookmark_context_menu::DISPOSITION_NEW_WINDOW;
    case IDC_VIV_BOOKMARK_BAR_OPEN_NEW_PRIVATE_WINDOW:
      return vivaldi::bookmark_context_menu::DISPOSITION_NEW_PRIVATE_WINDOW;
    default:
      return vivaldi::bookmark_context_menu::DISPOSITION_NONE;
  }
}

// Helper
static bool IsBackgroundCommand(int command) {
  switch(command) {
    case IDC_VIV_BOOKMARK_BAR_OPEN_BACKGROUND_TAB:
      return true;
    default:
      return false;
  }
}

// Helper
static vivaldi::bookmark_context_menu::Action CommandToAction(int command) {
  switch(command) {
    case IDC_VIV_BOOKMARK_BAR_ADD_ACTIVE_TAB:
      return vivaldi::bookmark_context_menu::ACTION_ADDACTIVETAB;
    case IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK:
      return vivaldi::bookmark_context_menu::ACTION_ADDBOOKMARK;
    case IDC_BOOKMARK_BAR_NEW_FOLDER:
      return vivaldi::bookmark_context_menu::ACTION_ADDFOLDER;
    case IDC_BOOKMARK_BAR_EDIT:
      return vivaldi::bookmark_context_menu::ACTION_EDIT;
    case IDC_CUT:
      return vivaldi::bookmark_context_menu::ACTION_CUT;
    case IDC_COPY:
      return vivaldi::bookmark_context_menu::ACTION_COPY;
    case IDC_PASTE:
      return vivaldi::bookmark_context_menu::ACTION_PASTE;
    default:
      return vivaldi::bookmark_context_menu::ACTION_NONE;
  }
}

BookmarkContextMenuAPI::BookmarkContextMenuAPI(BrowserContext* context) {}

BookmarkContextMenuAPI::~BookmarkContextMenuAPI() {}

// static
BrowserContextKeyedAPIFactory<BookmarkContextMenuAPI>*
    BookmarkContextMenuAPI::GetFactoryInstance() {
  return g_bookmark_context_menu.Pointer();
}

// static
void BookmarkContextMenuAPI::SendActivated(
    content::BrowserContext* browser_context,
    int id,
    int event_state) {
  vivaldi::bookmark_context_menu::Disposition disposition =
      vivaldi::bookmark_context_menu::DISPOSITION_SETTING;
  vivaldi::bookmark_context_menu::EventState state;
  state.ctrl = event_state & ui::EF_CONTROL_DOWN ? true : false;
  state.shift = event_state & ui::EF_SHIFT_DOWN ? true : false;
  state.alt = event_state & ui::EF_ALT_DOWN ? true : false;
  state.command = event_state & ui::EF_COMMAND_DOWN ? true : false;
  state.left = event_state & ui::EF_LEFT_MOUSE_BUTTON ? true : false;
  state.right = event_state & ui::EF_RIGHT_MOUSE_BUTTON ? true : false;
  state.center = event_state & ui::EF_MIDDLE_MOUSE_BUTTON ? true : false;
  ::vivaldi::BroadcastEvent(
      vivaldi::bookmark_context_menu::OnActivated::kEventName,
      vivaldi::bookmark_context_menu::OnActivated::Create(
          std::to_string(id), disposition, false, state),
      browser_context);
}

// static
void BookmarkContextMenuAPI::SendAction(
    content::BrowserContext* browser_context,
    int id,
    int index,
    int command) {
  // Test for "bookmark open commands" first.
  vivaldi::bookmark_context_menu::Disposition disposition =
      CommandToDisposition(command);
  if (disposition != vivaldi::bookmark_context_menu::DISPOSITION_NONE) {
    vivaldi::bookmark_context_menu::EventState state;
    ::vivaldi::BroadcastEvent(
        vivaldi::bookmark_context_menu::OnActivated::kEventName,
        vivaldi::bookmark_context_menu::OnActivated::Create(
            std::to_string(id), disposition, IsBackgroundCommand(command),
            state),
        browser_context);
    return;
  }
  // Then admin commands
  vivaldi::bookmark_context_menu::Action action = CommandToAction(command);
  if (action != vivaldi::bookmark_context_menu::ACTION_NONE) {
    ::vivaldi::BroadcastEvent(
        vivaldi::bookmark_context_menu::OnAction::kEventName,
        vivaldi::bookmark_context_menu::OnAction::Create(std::to_string(id),
                                                         action),
        browser_context);
  }
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

// static
void BookmarkContextMenuAPI::SendHover(
    content::BrowserContext* browser_context,
    const std::string& url) {
  BookmarkContextMenuAPI* api = GetFactoryInstance()->Get(browser_context);
  DCHECK(api);
  if (!api)
    return;
  if (api->hover_url_ != url) {
    api->hover_url_ = url;
    ::vivaldi::BroadcastEvent(
        vivaldi::bookmark_context_menu::OnHover::kEventName,
        vivaldi::bookmark_context_menu::OnHover::Create(url),
        browser_context);
  }
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

  // menuParams_ is a field in the function, not automatic variable, as
  // ::vivaldi::CreateVivaldiBookmarkMenu stores a pointer to it that must be
  // valid until at least BookmarkMenuClosed call.
  menuParams_.support.initIcons(params->properties.icons);
  menuParams_.sort_field = sortField;
  menuParams_.sort_order = sortOrder;

  menuParams_.siblings.reserve(params->properties.siblings.size());

  for (const vivaldi::bookmark_context_menu::FolderEntry& e:
       params->properties.siblings) {
    menuParams_.siblings.emplace_back();
    ::vivaldi::FolderEntry* entry = &menuParams_.siblings.back();
    if (!base::StringToInt64(e.id, &entry->id) || entry->id <= 0) {
      return RespondNow(Error("id is not a valid bookmark id - " + e.id));
    }
    entry->offset = e.offset;
    entry->folder_group = e.folder_group;
    entry->x = e.x;
    entry->y = e.y;
    entry->width = e.width;
    entry->height = e.height;
  }

  content::WebContents* web_contents = dispatcher()->GetAssociatedWebContents();
  if (web_contents) {
    ::vivaldi::ConvertBookmarkButtonRectToScreen(web_contents, menuParams_);
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

  const ::vivaldi::FolderEntry* entry = nullptr;
  for (const ::vivaldi::FolderEntry& e: menuParams_.siblings) {
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

  menuParams_.node = node;
  menuParams_.offset = entry->offset;
  menuParams_.folder_group = entry->folder_group;

  ::vivaldi::VivaldiBookmarkMenu* menu =
      ::vivaldi::CreateVivaldiBookmarkMenu(web_contents, menuParams_,
          gfx::Rect(entry->x, entry->y, entry->width, entry->height));
  if (!menu->CanShow()) {
    return "Can not show menu";
  }

  BookmarkContextMenuAPI::SendOpen(browser_context(), node_id);
  menu->set_observer(this);
  menu->Show();

  return "";
}

void BookmarkContextMenuShowFunction::BookmarkMenuClosed(
    ::vivaldi::VivaldiBookmarkMenu* menu) {
  namespace Results = vivaldi::bookmark_context_menu::Show::Results;

  BookmarkContextMenuAPI::SendClose(browser_context());
  Respond(ArgumentList(Show::Results::Create()));
  Release();
}

}  // namespace extensions
