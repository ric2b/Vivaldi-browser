//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#include "extensions/api/menubar_menu/menubar_menu_api.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/base/accelerators/accelerator.h"

namespace extensions {

// helper
static bool IsBackgroundCommand(int command) {
  switch(command) {
    case IDC_VIV_BOOKMARK_BAR_OPEN_BACKGROUND_TAB:
      return true;
    default:
      return false;
  }
}

// Helper
static vivaldi::menubar_menu::Disposition CommandToDisposition(int command) {
  switch (command) {
    case IDC_VIV_BOOKMARK_BAR_OPEN_CURRENT_TAB:
      return vivaldi::menubar_menu::DISPOSITION_CURRENT;
    case IDC_VIV_BOOKMARK_BAR_OPEN_NEW_TAB:
    case IDC_VIV_BOOKMARK_BAR_OPEN_BACKGROUND_TAB:
      return vivaldi::menubar_menu::DISPOSITION_NEW_TAB;
    case IDC_VIV_BOOKMARK_BAR_OPEN_NEW_WINDOW:
      return vivaldi::menubar_menu::DISPOSITION_NEW_WINDOW;
    case IDC_VIV_BOOKMARK_BAR_OPEN_NEW_PRIVATE_WINDOW:
      return vivaldi::menubar_menu::DISPOSITION_NEW_PRIVATE_WINDOW;
    default:
      return vivaldi::menubar_menu::DISPOSITION_NONE;
  }
}

// Helper
static vivaldi::menubar_menu::BookmarkCommand CommandToAction(int command) {
  switch (command) {
    case IDC_VIV_BOOKMARK_BAR_ADD_ACTIVE_TAB:
      return vivaldi::menubar_menu::BOOKMARK_COMMAND_ADDACTIVETAB;
    case IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK:
      return vivaldi::menubar_menu::BOOKMARK_COMMAND_ADDBOOKMARK;
    case IDC_BOOKMARK_BAR_NEW_FOLDER:
      return vivaldi::menubar_menu::BOOKMARK_COMMAND_ADDFOLDER;
    case IDC_BOOKMARK_BAR_EDIT:
      return vivaldi::menubar_menu::BOOKMARK_COMMAND_EDIT;
    case IDC_CUT:
      return vivaldi::menubar_menu::BOOKMARK_COMMAND_CUT;
    case IDC_COPY:
      return vivaldi::menubar_menu::BOOKMARK_COMMAND_COPY;
    case IDC_PASTE:
      return vivaldi::menubar_menu::BOOKMARK_COMMAND_PASTE;
    default:
      return vivaldi::menubar_menu::BOOKMARK_COMMAND_NONE;
  }
}

// Helper
static vivaldi::menubar_menu::EventState FlagToEventState(int flag) {
  vivaldi::menubar_menu::EventState event_state;
  event_state.ctrl = flag & ui::EF_CONTROL_DOWN ? true : false;
  event_state.shift = flag & ui::EF_SHIFT_DOWN ? true : false;
  event_state.alt = flag & ui::EF_ALT_DOWN ? true : false;
  event_state.command = flag & ui::EF_COMMAND_DOWN ? true : false;
  event_state.left = flag & ui::EF_LEFT_MOUSE_BUTTON ? true : false;
  event_state.right = flag & ui::EF_RIGHT_MOUSE_BUTTON ? true : false;
  event_state.center = flag & ui::EF_MIDDLE_MOUSE_BUTTON ? true : false;
  return event_state;
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    MenubarMenuAPI>>::DestructorAtExit g_menubar_menu =
        LAZY_INSTANCE_INITIALIZER;

MenubarMenuAPI::MenubarMenuAPI(BrowserContext* context) {}

MenubarMenuAPI::~MenubarMenuAPI() {}

// static
BrowserContextKeyedAPIFactory<MenubarMenuAPI>*
		MenubarMenuAPI::GetFactoryInstance() {
  return g_menubar_menu.Pointer();
}

// static
void MenubarMenuAPI::SendAction(content::BrowserContext* browser_context,
                                int command,
                                int event_state) {
	vivaldi::menubar_menu::Action action;
  // Convert to api id before sending to JS.
	action.id = command - IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST - 1;
  action.state = FlagToEventState(event_state);
  ::vivaldi::BroadcastEvent(
      vivaldi::menubar_menu::OnAction::kEventName,
      vivaldi::menubar_menu::OnAction::Create(action),
      browser_context);
}

// static
void MenubarMenuAPI::SendOpenBookmark(content::BrowserContext* browser_context,
                                      int64_t bookmark_id,
                                      int event_state) {
  vivaldi::menubar_menu::BookmarkAction action;
  action.id = std::to_string(bookmark_id);
  action.disposition = vivaldi::menubar_menu::DISPOSITION_SETTING;
  action.background = false;
  action.state = FlagToEventState(event_state);
  ::vivaldi::BroadcastEvent(
      vivaldi::menubar_menu::OnOpenBookmark::kEventName,
      vivaldi::menubar_menu::OnOpenBookmark::Create(action),
      browser_context);
}

// static
void MenubarMenuAPI::SendBookmarkAction(content::BrowserContext* browser_context,
                                        int64_t bookmark_id,
                                        int command) {
  // Some commands will open a bookmark while the rest are managing actions. If
  // we have a disposition the bookmark should be opened.
  vivaldi::menubar_menu::Disposition disposition =
      CommandToDisposition(command);
  if (disposition != vivaldi::menubar_menu::DISPOSITION_NONE) {
    vivaldi::menubar_menu::BookmarkAction action;
    action.id = std::to_string(bookmark_id);
    action.disposition = disposition;
    action.background = IsBackgroundCommand(command);
    ::vivaldi::BroadcastEvent(
      vivaldi::menubar_menu::OnOpenBookmark::kEventName,
      vivaldi::menubar_menu::OnOpenBookmark::Create(action),
      browser_context);
  } else {
    vivaldi::menubar_menu::BookmarkAction action;
    action.id = std::to_string(bookmark_id);
    action.command = CommandToAction(command);
    ::vivaldi::BroadcastEvent(
      vivaldi::menubar_menu::OnBookmarkAction::kEventName,
      vivaldi::menubar_menu::OnBookmarkAction::Create(action),
      browser_context);
  }
}

// static
void MenubarMenuAPI::SendOpen(
    content::BrowserContext* browser_context, int menu_id) {
  ::vivaldi::BroadcastEvent(
      vivaldi::menubar_menu::OnOpen::kEventName,
      vivaldi::menubar_menu::OnOpen::Create(menu_id),
      browser_context);
}

// static
void MenubarMenuAPI::SendClose(
    content::BrowserContext* browser_context) {
  ::vivaldi::BroadcastEvent(vivaldi::menubar_menu::OnClose::kEventName,
                            vivaldi::menubar_menu::OnClose::Create(),
                            browser_context);
}

// static
void MenubarMenuAPI::SendHover(
    content::BrowserContext* browser_context,
    const std::string& url) {
  MenubarMenuAPI* api = GetFactoryInstance()->Get(browser_context);
  DCHECK(api);
  if (!api)
    return;
  if (api->hover_url_ != url) {
    api->hover_url_ = url;
    ::vivaldi::BroadcastEvent(
        vivaldi::menubar_menu::OnHover::kEventName,
        vivaldi::menubar_menu::OnHover::Create(url),
        browser_context);
  }
}

// static
void MenubarMenuAPI::SendError(
    content::BrowserContext* browser_context,
    const std::string& text) {
  ::vivaldi::BroadcastEvent(vivaldi::menubar_menu::OnError::kEventName,
                            vivaldi::menubar_menu::OnError::Create(text),
                            browser_context);
}

#if defined(OS_MACOSX)
// Not used on Mac but we still need a stub
MenubarMenuShowFunction::MenubarMenuShowFunction() = default;
MenubarMenuShowFunction::~MenubarMenuShowFunction() = default;
ExtensionFunction::ResponseAction MenubarMenuShowFunction::Run() {
  return RespondNow(Error("Not implemented on Mac"));
}
#else
MenubarMenuShowFunction::MenubarMenuShowFunction()
  :menuParams_(this) {
  }

MenubarMenuShowFunction::~MenubarMenuShowFunction() = default;

ExtensionFunction::ResponseAction MenubarMenuShowFunction::Run() {
	using vivaldi::menubar_menu::Show::Params;
  namespace Results = vivaldi::menubar_menu::Show::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Set up needed information for the menu bar to request menus on demand.
  menuParams_.siblings.reserve(params->properties.siblings.size());
  for (const vivaldi::menubar_menu::Menu& m: params->properties.siblings) {
    menuParams_.siblings.emplace_back();
    ::vivaldi::MenubarMenuEntry* entry = &menuParams_.siblings.back();
    entry->id = m.id;
    entry->rect = gfx::Rect(m.rect.x, m.rect.y, m.rect.width, m.rect.height);
  }

  content::WebContents* web_contents = dispatcher()->GetAssociatedWebContents();
  if (web_contents) {
    ::vivaldi::ConvertMenubarButtonRectToScreen(web_contents, menuParams_);
  }

  // We have not yet created any menu content. This will be done on a per menu
  // basis when and if the menu is needed.
  std::string error = Open(params->properties.id);
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  AddRef();
  return RespondLater();
}

std::string MenubarMenuShowFunction::Open(int id) {
  content::WebContents* web_contents = dispatcher()->GetAssociatedWebContents();
  if (!web_contents) {
    return "No WebContents";
  }

  // Range check
  bool match = false;
  for (const ::vivaldi::MenubarMenuEntry& entry: menuParams_.siblings) {
    if (entry.id == id) {
      match = true;
      break;
    }
  }
  if (!match) {
    return "Menu id out of range";
  }

  menu_.reset(::vivaldi::CreateVivaldiMenubarMenu(web_contents, menuParams_,
                                                  id));
  if (!menu_->CanShow()) {
    return "Can not show menu";
  }

  menu_->Show();

  return "";
}

std::string MenubarMenuShowFunction::PopulateModel(
    vivaldi::menubar_menu::Show::Params* params,
    int menu_id,
    const std::vector<vivaldi::menubar_menu::Element>& list,
    ui::SimpleMenuModel* menu_model) {
  for (const vivaldi::menubar_menu::Element& child: list) {
    if (child.item) {
      int id = child.item->id + IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST + 1;
      const base::string16 label = base::UTF8ToUTF16(child.item->label);
      if (child.children) {
        ui::SimpleMenuModel* child_menu_model = new ui::SimpleMenuModel(nullptr);
        models_.push_back(base::WrapUnique(child_menu_model));
        menu_model->AddSubMenu(id, label, child_menu_model);
        PopulateModel(params, menu_id, *child.children, child_menu_model);
      } else {
        if (child.item->type == vivaldi::menubar_menu::ITEM_TYPE_CHECKBOX) {
          menu_model->AddCheckItem(id, label);
          id_to_checked_map_[id] = child.item->checked && *child.item->checked;
        } else if (child.item->type ==
                   vivaldi::menubar_menu::ITEM_TYPE_RADIOBUTTON) {
          if (!child.item->group.get()) {
            return "Radio button added without group";
          }
          menu_model->AddRadioItem(id, label, *child.item->group.get());
          id_to_checked_map_[id] = child.item->checked && *child.item->checked;
        } else {
          menu_model->AddItem(id, label);
        }
        if (child.item->shortcut) {
          id_to_accelerator_map_[id] = ::vivaldi::ParseShortcut(
              *child.item->shortcut, true);
        }

        if (child.item->icon && child.item->icon->length() > 0) {
          std::string png_data;
          if (base::Base64Decode(*child.item->icon, &png_data)) {
            gfx::Image img = gfx::Image::CreateFrom1xPNGBytes(
                reinterpret_cast<const unsigned char*>(png_data.c_str()),
                png_data.length());
            menu_model->SetIcon(menu_model->GetIndexOfCommandId(id), img);
          }
        }
        if (child.item->url && child.item->url->length() > 0) {
          id_to_url_map_[id] = *child.item->url;
        }
        if (child.item->persistent && *child.item->persistent == true) {
          id_to_persistent_map_[id] = true;
        }
      }
    } else if (child.separator) {
      menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    } else if (child.container) {
      if (child.container->type ==
          vivaldi::menubar_menu::CONTAINER_TYPE_BOOKMARKS) {
        if (bookmark_menu_container_) {
          return "Only one bookmark container supported";
        }
        bookmark_menu_id_ = menu_id;
        bookmark_menu_container_.reset(
            new ::vivaldi::BookmarkMenuContainer(this));
        bookmark_menu_container_->siblings.reserve(1);
        bookmark_menu_container_->siblings.emplace_back();
        ::vivaldi::BookmarkMenuContainerEntry* sibling =
            &bookmark_menu_container_->siblings.back();
        if (!base::StringToInt64(child.container->id, &sibling->id) ||
             sibling->id <= 0) {
          return "Illegal bookmark id";
        }
        sibling->offset = child.container->offset;
        sibling->folder_group =
            child.container->group_folders && *child.container->group_folders;
        bookmark_menu_container_->support.initIcons(params->properties.icons);
        switch (child.container->sort_field) {
          case vivaldi::menubar_menu::SORT_FIELD_NONE:
            bookmark_menu_container_->sort_field =
                ::vivaldi::BookmarkSorter::FIELD_NONE;
          break;
          case vivaldi::menubar_menu::SORT_FIELD_TITLE:
            bookmark_menu_container_->sort_field =
                ::vivaldi::BookmarkSorter::FIELD_TITLE;
          break;
          case vivaldi::menubar_menu::SORT_FIELD_URL:
            bookmark_menu_container_->sort_field =
                ::vivaldi::BookmarkSorter::FIELD_URL;
          break;
          case vivaldi::menubar_menu::SORT_FIELD_NICKNAME:
            bookmark_menu_container_->sort_field =
                ::vivaldi::BookmarkSorter::FIELD_NICKNAME;
          break;
          case vivaldi::menubar_menu::SORT_FIELD_DESCRIPTION:
            bookmark_menu_container_->sort_field =
                ::vivaldi::BookmarkSorter::FIELD_NICKNAME;
          break;
          case vivaldi::menubar_menu::SORT_FIELD_DATEADDED:
            bookmark_menu_container_->sort_field =
                ::vivaldi::BookmarkSorter::FIELD_DATEADDED;
          break;
        };
        switch (child.container->sort_order) {
          case vivaldi::menubar_menu::SORT_ORDER_NONE:
             bookmark_menu_container_->sort_order =
                ::vivaldi::BookmarkSorter::ORDER_NONE;
          break;
          case vivaldi::menubar_menu::SORT_ORDER_ASCENDING:
             bookmark_menu_container_->sort_order =
                ::vivaldi::BookmarkSorter::ORDER_ASCENDING;
          break;
          case vivaldi::menubar_menu::SORT_ORDER_DESCENDING:
             bookmark_menu_container_->sort_order =
                ::vivaldi::BookmarkSorter::ORDER_DESCENDING;
          break;
        };
      } else {
        return "Unknown container element";
      }
    } else {
      return "Unknown menu element";
    }
	}
  SanitizeModel(menu_model);
  return "";
}

void MenubarMenuShowFunction::PopulateModel(int menu_id,
                                            ui::SimpleMenuModel** menu_model) {
  using vivaldi::menubar_menu::Show::Params;

  *menu_model = new ui::SimpleMenuModel(nullptr);
  models_.push_back(base::WrapUnique(*menu_model)); // We own the model.

  std::unique_ptr<Params> params = Params::Create(*args_);
  std::vector<vivaldi::menubar_menu::Menu>& list = params->properties.siblings;
  for (const vivaldi::menubar_menu::Menu& sibling: list) {
    if (sibling.id == menu_id) {
      std::string error = PopulateModel(params.get(), sibling.id,
          sibling.children, *menu_model);
      if (!error.empty()) {
         MenubarMenuAPI::SendError(browser_context(), error);
      }
      break;
    }
  }
}

void MenubarMenuShowFunction::SanitizeModel(ui::SimpleMenuModel* menu_model) {
  for (int i = menu_model->GetItemCount() - 1; i >= 0; i--) {
    if (menu_model->GetTypeAt(i) == ui::MenuModel::TYPE_SEPARATOR) {
      menu_model->RemoveItemAt(i);
    } else {
      break;
    }
  }
}

void MenubarMenuShowFunction::OnMenuOpened(int menu_id) {
  MenubarMenuAPI::SendOpen(browser_context(), menu_id);
}

void MenubarMenuShowFunction::OnMenuClosed() {
  namespace Results = vivaldi::menubar_menu::Show::Results;
  MenubarMenuAPI::SendClose(browser_context());
  Respond(ArgumentList(Show::Results::Create()));
  Release();
}

void MenubarMenuShowFunction::OnAction(int command, int event_state) {
  MenubarMenuAPI::SendAction(browser_context(), command, event_state);
}

void MenubarMenuShowFunction::OnHover(const std::string& url) {
  MenubarMenuAPI::SendHover(browser_context(), url);
}

void MenubarMenuShowFunction::OnOpenBookmark(int64_t bookmark_id,
                                             int event_state) {
  MenubarMenuAPI::SendOpenBookmark(browser_context(), bookmark_id, event_state);
}

void MenubarMenuShowFunction::OnBookmarkAction(int64_t bookmark_id,
                                               int command) {
  MenubarMenuAPI::SendBookmarkAction(browser_context(), bookmark_id, command);
}

bool MenubarMenuShowFunction::IsBookmarkMenu(int menu_id) {
  return bookmark_menu_id_ == menu_id;
}

bool MenubarMenuShowFunction::IsItemChecked(int id) {
  std::map<int, bool>::iterator it = id_to_checked_map_.find(id);
  return it != id_to_checked_map_.end() ? it->second : false;
}

bool MenubarMenuShowFunction::IsItemPersistent(int id) {
  std::map<int, bool>::iterator it = id_to_persistent_map_.find(id);
  return it != id_to_persistent_map_.end() ? it->second : false;
}

bool MenubarMenuShowFunction::GetAccelerator(int id,
    ui::Accelerator* accelerator) {
  std::map<int, ui::Accelerator>::iterator it = id_to_accelerator_map_.find(id);
  if (it == id_to_accelerator_map_.end()) {
    return false;
  } else {
    *accelerator = it->second;
    return true;
  }
}

bool MenubarMenuShowFunction::GetUrl(int id, std::string* url) {
  std::map<int, std::string>::iterator it = id_to_url_map_.find(id);
  if (it == id_to_url_map_.end()) {
    return false;
  } else {
    *url = it->second;
    return true;
  }
}

::vivaldi::BookmarkMenuContainer*
    MenubarMenuShowFunction::GetBookmarkMenuContainer() {
  return bookmark_menu_container_.get();
}

#endif

}  // namespace extensions
