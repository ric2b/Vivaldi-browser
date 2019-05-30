//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#include "extensions/api/bookmark_context_menu/bookmark_context_menu_api.h"

#include "browser/menus/vivaldi_menu_enums.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "extensions/browser/extension_function_dispatcher.h"
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

BookmarkContextMenuAPI::BookmarkContextMenuAPI(BrowserContext* context)
  : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(
      this, vivaldi::bookmark_context_menu::OnActivated::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::bookmark_context_menu::OnAction::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::bookmark_context_menu::OnOpen::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::bookmark_context_menu::OnClose::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::bookmark_context_menu::OnHover::kEventName);
}

BookmarkContextMenuAPI::~BookmarkContextMenuAPI() {}

// static
BrowserContextKeyedAPIFactory<BookmarkContextMenuAPI>*
    BookmarkContextMenuAPI::GetFactoryInstance() {
  return g_bookmark_context_menu.Pointer();
}

void BookmarkContextMenuAPI::OnListenerAdded(const EventListenerInfo& details) {
  event_router_.reset(new BookmarkContextMenuEventRouter(
      Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void BookmarkContextMenuAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void BookmarkContextMenuAPI::OnActivated(int id, int event_state) {
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
  if (event_router_) {
    event_router_->DispatchEvent(
        vivaldi::bookmark_context_menu::OnActivated::kEventName,
        vivaldi::bookmark_context_menu::OnActivated::Create(
            std::to_string(id), disposition, false, state));
  }
}

void BookmarkContextMenuAPI::OnAction(int id, int index, int command) {
  if (event_router_) {
    // Test for "bookmark open commands" first.
    vivaldi::bookmark_context_menu::Disposition disposition =
      CommandToDisposition(command);
    if (disposition != vivaldi::bookmark_context_menu::DISPOSITION_NONE) {
      vivaldi::bookmark_context_menu::EventState state;
      event_router_->DispatchEvent(
        vivaldi::bookmark_context_menu::OnActivated::kEventName,
        vivaldi::bookmark_context_menu::OnActivated::Create(std::to_string(id),
            disposition, IsBackgroundCommand(command), state));
      return;
    }
    // Then admin commands
    vivaldi::bookmark_context_menu::Action action = CommandToAction(command);
    if (action != vivaldi::bookmark_context_menu::ACTION_NONE) {
       event_router_->DispatchEvent(
          vivaldi::bookmark_context_menu::OnAction::kEventName,
          vivaldi::bookmark_context_menu::OnAction::Create(std::to_string(id),
              action));
    }
  }
}

void BookmarkContextMenuAPI::OnOpen() {
  if (event_router_) {
    event_router_->DispatchEvent(
      vivaldi::bookmark_context_menu::OnOpen::kEventName,
      vivaldi::bookmark_context_menu::OnOpen::Create());
  }
}

void BookmarkContextMenuAPI::OnClose() {
  if (event_router_) {
    event_router_->DispatchEvent(
      vivaldi::bookmark_context_menu::OnClose::kEventName,
      vivaldi::bookmark_context_menu::OnClose::Create());
  }
}

void BookmarkContextMenuAPI::OnHover(const std::string& url) {
  if (hover_url_ != url) {
    hover_url_ = url;
    if (event_router_) {
      event_router_->DispatchEvent(
          vivaldi::bookmark_context_menu::OnHover::kEventName,
          vivaldi::bookmark_context_menu::OnHover::Create(hover_url_));
    }
  }
}


BookmarkContextMenuEventRouter::BookmarkContextMenuEventRouter(Profile* profile)
  : browser_context_(profile) {}

BookmarkContextMenuEventRouter::~BookmarkContextMenuEventRouter() {}

void BookmarkContextMenuEventRouter::DispatchEvent(
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->BroadcastEvent(base::WrapUnique(
        new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                              event_name, std::move(event_args))));
  }
}


bool BookmarkContextMenuShowFunction::RunAsync() {
  content::WebContents* web_contents = dispatcher()->GetAssociatedWebContents();
  if (web_contents) {
    profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());

    params_ = Show::Params::Create(*args_);
    EXTENSION_FUNCTION_VALIDATE(params_.get());

    bookmarks::BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(
            web_contents->GetBrowserContext());
    const bookmarks::BookmarkNode* node = params_->properties.id.empty() ?
      nullptr : bookmarks::GetBookmarkNodeByID(model,
                    std::atoi(params_->properties.id.c_str()));
    if (node) {

      ::vivaldi::BookmarkSorter::SortField sortField;
      switch (params_->properties.sort_field) {
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
      switch (params_->properties.sort_order) {
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

      menuParams_.node = node;
      menuParams_.offset = params_->properties.offset;
      menuParams_.folder_group = params_->properties.folder_group;
      menuParams_.support.initIcons(params_->properties.icons);
      menuParams_.sort_field = sortField;
      menuParams_.sort_order = sortOrder;

      ::vivaldi::VivaldiBookmarkMenu* menu =
          ::vivaldi::CreateVivaldiBookmarkMenu(web_contents, menuParams_,
              gfx::Rect(params_->properties.x, params_->properties.y,
                        params_->properties.width, params_->properties.height));
      if (menu->CanShow()) {
        BookmarkContextMenuAPI::GetFactoryInstance()->Get(profile_)->OnOpen();
        AddRef();
        menu->set_observer(this);
        menu->Show();
        return true;
      }
    }
  }

  Respond(ArgumentList(Show::Results::Create()));
  return true;
}

void BookmarkContextMenuShowFunction::BookmarkMenuClosed(
    ::vivaldi::VivaldiBookmarkMenu* menu) {
  BookmarkContextMenuAPI::GetFactoryInstance()->Get(profile_)->OnClose();
  Respond(ArgumentList(Show::Results::Create()));
  Release();
}

BookmarkContextMenuShowFunction::BookmarkContextMenuShowFunction()
  :profile_(nullptr) {
}

BookmarkContextMenuShowFunction::~BookmarkContextMenuShowFunction() {
}

}  // namespace extensions
