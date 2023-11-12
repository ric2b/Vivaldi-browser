//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#include "extensions/api/menubar_menu/menubar_menu_api.h"

#include "base/base64.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/simple_menu_model.h"

#include "browser/menus/vivaldi_menu_enums.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/vivaldi_browser_window.h"

namespace extensions {

// helper
static bool IsBackgroundCommand(int command) {
  switch (command) {
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
    case IDC_VIV_BOOKMARK_BAR_NEW_SEPARATOR:
      return vivaldi::menubar_menu::BOOKMARK_COMMAND_ADDSEPARATOR;
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

static base::LazyInstance<BrowserContextKeyedAPIFactory<MenubarMenuAPI>>::
    DestructorAtExit g_menubar_menu = LAZY_INSTANCE_INITIALIZER;

MenubarMenuAPI::MenubarMenuAPI(content::BrowserContext* context) {}

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
  action.id = command - IDC_VIV_MENU_FIRST - 1;
  action.state = FlagToEventState(event_state);
  ::vivaldi::BroadcastEvent(vivaldi::menubar_menu::OnAction::kEventName,
                            vivaldi::menubar_menu::OnAction::Create(action),
                            browser_context);
}

// static
void MenubarMenuAPI::SendOpenBookmark(content::BrowserContext* browser_context,
                                      int window_id,
                                      int64_t bookmark_id,
                                      int event_state) {
  vivaldi::menubar_menu::BookmarkAction action;
  action.id = std::to_string(bookmark_id);
  action.disposition = vivaldi::menubar_menu::DISPOSITION_SETTING;
  action.background = false;
  action.state = FlagToEventState(event_state);
  ::vivaldi::BroadcastEvent(
      vivaldi::menubar_menu::OnOpenBookmark::kEventName,
      vivaldi::menubar_menu::OnOpenBookmark::Create(window_id, action),
      browser_context);
}

// static
void MenubarMenuAPI::SendBookmarkAction(
    content::BrowserContext* browser_context,
    int window_id,
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
        vivaldi::menubar_menu::OnOpenBookmark::Create(window_id, action),
        browser_context);
  } else {
    vivaldi::menubar_menu::BookmarkAction action;
    action.id = std::to_string(bookmark_id);
    action.command = CommandToAction(command);
    ::vivaldi::BroadcastEvent(
        vivaldi::menubar_menu::OnBookmarkAction::kEventName,
        vivaldi::menubar_menu::OnBookmarkAction::Create(window_id, action),
        browser_context);
  }
}

// static
void MenubarMenuAPI::SendOpen(content::BrowserContext* browser_context,
                              int menu_id) {
  ::vivaldi::BroadcastEvent(vivaldi::menubar_menu::OnOpen::kEventName,
                            vivaldi::menubar_menu::OnOpen::Create(menu_id),
                            browser_context);
}

// static
void MenubarMenuAPI::SendClose(content::BrowserContext* browser_context) {
  ::vivaldi::BroadcastEvent(vivaldi::menubar_menu::OnClose::kEventName,
                            vivaldi::menubar_menu::OnClose::Create(),
                            browser_context);
}

// static
void MenubarMenuAPI::SendHover(content::BrowserContext* browser_context,
                               int window_id,
                               const std::string& url) {
  MenubarMenuAPI* api = GetFactoryInstance()->Get(browser_context);
  DCHECK(api);
  if (!api)
    return;
  if (api->hover_url_ != url) {
    api->hover_url_ = url;
    ::vivaldi::BroadcastEvent(vivaldi::menubar_menu::OnHover::kEventName,
                              vivaldi::menubar_menu::OnHover::Create(window_id,
                                                                     url),
                              browser_context);
  }
}

// static
void MenubarMenuAPI::SendError(content::BrowserContext* browser_context,
                               const std::string& text) {
  ::vivaldi::BroadcastEvent(vivaldi::menubar_menu::OnError::kEventName,
                            vivaldi::menubar_menu::OnError::Create(text),
                            browser_context);
}

#if BUILDFLAG(IS_MAC)
// Not used on Mac but we still need a stub
MenubarMenuShowFunction::MenubarMenuShowFunction() = default;
MenubarMenuShowFunction::~MenubarMenuShowFunction() = default;
ExtensionFunction::ResponseAction MenubarMenuShowFunction::Run() {
  return RespondNow(Error("Not implemented on Mac"));
}
#else
MenubarMenuShowFunction::MenubarMenuShowFunction() : menuParams_(this) {}

MenubarMenuShowFunction::~MenubarMenuShowFunction() = default;

ExtensionFunction::ResponseAction MenubarMenuShowFunction::Run() {
  using vivaldi::menubar_menu::Show::Params;
  namespace Results = vivaldi::menubar_menu::Show::Results;

  params_ = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  // Set up needed information for the menu bar to request menus on demand.
  menuParams_.siblings.reserve(params_->properties.siblings.size());
  for (const vivaldi::menubar_menu::Menu& m : params_->properties.siblings) {
    menuParams_.siblings.emplace_back();
    ::vivaldi::MenubarMenuEntry* entry = &menuParams_.siblings.back();
    entry->id = m.id;
    entry->rect = gfx::Rect(m.rect.x, m.rect.y, m.rect.width, m.rect.height);
  }

  VivaldiBrowserWindow* window =
      VivaldiBrowserWindow::FromId(params_->properties.window_id);
  if (!window) {
    return RespondNow(Error("No such window"));
  }
  ::vivaldi::ConvertMenubarButtonRectToScreen(window->web_contents(),
                                              menuParams_);

  // We have not yet created any menu content. This will be done on a per menu
  // basis when and if the menu is needed.
  std::string error = Open(window->web_contents(), params_->properties.id);
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  AddRef();
  return RespondLater();
}

std::string MenubarMenuShowFunction::Open(content::WebContents* web_contents,
                                          int id) {
  // Range check
  bool match = false;
  for (const ::vivaldi::MenubarMenuEntry& entry : menuParams_.siblings) {
    if (entry.id == id) {
      match = true;
      break;
    }
  }
  if (!match) {
    return "Menu id out of range";
  }

  menu_.reset(
      ::vivaldi::CreateVivaldiMenubarMenu(web_contents, menuParams_, id));
  if (!menu_->CanShow()) {
    return "Can not show menu";
  }

  menu_->Show();

  return "";
}

std::string MenubarMenuShowFunction::PopulateModel(
    absl::optional<vivaldi::menubar_menu::Show::Params> &params,
    int menu_id,
    bool dark_text_color,
    const std::vector<Element>& list,
    ui::SimpleMenuModel* menu_model) {
  bool prev_is_bookmarks = false;
  namespace menubar_menu = extensions::vivaldi::menubar_menu;
  for (const Element& child : list) {
    if (child.item) {
      prev_is_bookmarks = false;
      const Item& item = *child.item;
      int id = item.id + IDC_VIV_MENU_FIRST + 1;
      const std::u16string label = base::UTF8ToUTF16(item.name);
      switch (item.type) {
        case menubar_menu::ITEM_TYPE_COMMAND:
          menu_model->AddItem(id, label);
          if (item.enabled && !*item.enabled) {
            id_to_disabled_map_[id] = true;
          }
          break;
        case menubar_menu::ITEM_TYPE_CHECKBOX:
          menu_model->AddCheckItem(id, label);
          id_to_checked_map_[id] = item.checked && *item.checked;
          if (item.enabled && !*item.enabled) {
            id_to_disabled_map_[id] = true;
          }
          break;
        case menubar_menu::ITEM_TYPE_RADIO:
          if (!item.radiogroup.has_value()) {
            return "Radio button added without group";
          }
          menu_model->AddRadioItem(id, label, item.radiogroup.value());
          id_to_checked_map_[id] = item.checked.value_or(false);
          if (item.enabled && !*item.enabled) {
            id_to_disabled_map_[id] = true;
          }
          break;
        case menubar_menu::ITEM_TYPE_FOLDER: {
          // We create the SimpleMenuModel sub menu but do not populate it. That
          // will be done in PopulateSubmodel() by the calling menu code when
          // and if this sub menu will be shown to the user.
          if (item.selected.value_or(false)) {
            if (selected_menu_id_ != -1) {
              return "Only one menu item can be selected";
            }
            selected_menu_id_ = id;
          }
          ui::SimpleMenuModel* child_menu_model =
              new ui::SimpleMenuModel(nullptr);
          models_.push_back(base::WrapUnique(child_menu_model));
          menu_model->AddSubMenu(id, label, child_menu_model);
          if (child.children.has_value())
            id_to_elementvector_map_[id] = &child.children.value();
          break;
        }
        case menubar_menu::ITEM_TYPE_NONE:
          return "Item type missing";
      }
      if (item.shortcut) {
        id_to_accelerator_map_[id] =
            ::vivaldi::ParseShortcut(*item.shortcut, true);
      }
      if (item.url && !item.url->empty()) {
        id_to_url_map_[id] = *item.url;
      }
      if (item.persistent && *item.persistent == true) {
        id_to_persistent_map_[id] = true;
      }
      if (item.icons) {
        if (item.icons->size() != 2) {
          return "Wrong number of icons";
        } else {
          const std::string* icon = &item.icons->at(dark_text_color ? 0 : 1);
          if (icon->length() > 0) {
            std::string png_data;
            if (base::Base64Decode(*icon, &png_data)) {
              gfx::Image img = gfx::Image::CreateFrom1xPNGBytes(
                  reinterpret_cast<const unsigned char*>(png_data.c_str()),
                  png_data.length());
              menu_model->SetIcon(menu_model->GetIndexOfCommandId(id).value(),
                                  ui::ImageModel::FromImage(img));
            }
          }
        }
      }
    } else if (child.separator) {
      // All container types except bookmarks are expanded in JS. For bookmarks
      // expansion happens in C++ code just before a menu is shown. This causes
      // a problem for separators since the menu model prevents adding multiple
      // separators after another which we may do since the container content
      // is expanded later. We let the container expansion code add a separator
      // if needed (Note: we reset this tweak at the end of this function if
      // the bookmark container turns out to be the last element in the menu).
      if (prev_is_bookmarks) {
        bookmark_menu_container_->siblings[0].tweak_separator = true;
      } else {
        menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
      }
    } else if (child.container) {
      if (child.container->type ==
          vivaldi::menubar_menu::CONTAINER_TYPE_BOOKMARKS) {
        if (bookmark_menu_container_) {
          return "Only one bookmark container supported";
        }
        prev_is_bookmarks = true;
        bookmark_menu_id_ = menu_id;
        bookmark_menu_container_.reset(
            new ::vivaldi::BookmarkMenuContainer(this));
        switch (child.container->edge) {
          case vivaldi::menubar_menu::EDGE_ABOVE:
            bookmark_menu_container_->edge =
                ::vivaldi::BookmarkMenuContainer::Above;
            break;
          case vivaldi::menubar_menu::EDGE_BELOW:
            bookmark_menu_container_->edge =
                ::vivaldi::BookmarkMenuContainer::Below;
            break;
          default:
            bookmark_menu_container_->edge =
                ::vivaldi::BookmarkMenuContainer::Off;
            break;
        };
        bookmark_menu_container_->siblings.reserve(1);
        bookmark_menu_container_->siblings.emplace_back();
        ::vivaldi::BookmarkMenuContainerEntry* sibling =
            &bookmark_menu_container_->siblings.back();
        if (!base::StringToInt64(child.container->id, &sibling->id) ||
            sibling->id <= 0) {
          return "Illegal bookmark id";
        }
        sibling->offset = child.container->offset;
        sibling->menu_index = menu_model->GetItemCount();
        sibling->tweak_separator = false;
        sibling->folder_group = child.container->group_folders;
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
  if (prev_is_bookmarks) {
    bookmark_menu_container_->siblings[0].tweak_separator = false;
  } else {
    SanitizeModel(menu_model);
  }

  return "";
}

// Called by menu code to populate the top level of a menu model
void MenubarMenuShowFunction::PopulateModel(int menu_id,
                                            bool dark_text_color,
                                            ui::MenuModel** menu_model) {
  using vivaldi::menubar_menu::Show::Params;

  ui::SimpleMenuModel* simple_menu_model = new ui::SimpleMenuModel(nullptr);
  models_.push_back(base::WrapUnique(simple_menu_model));  // We own the model.
  *menu_model = simple_menu_model;

  std::vector<vivaldi::menubar_menu::Menu>& list = params_->properties.siblings;
  for (const vivaldi::menubar_menu::Menu& sibling : list) {
    if (sibling.id == menu_id) {
      std::string error =
          PopulateModel(params_, sibling.id, dark_text_color,
                        sibling.children, simple_menu_model);
      if (!error.empty()) {
        MenubarMenuAPI::SendError(browser_context(), error);
      }
      break;
    }
  }
}

// Called by menu code to populate a sub menu model of an existing menu model
void MenubarMenuShowFunction::PopulateSubmodel(int menu_id,
                                               bool dark_text_color,
                                               ui::MenuModel* menu_model) {
  using vivaldi::menubar_menu::Show::Params;

  // Avoids a static_cast and takes no time
  ui::SimpleMenuModel* simple_menu_model = nullptr;
  for (std::unique_ptr<ui::SimpleMenuModel>& model : models_) {
    if (model.get() == menu_model) {
      simple_menu_model = model.get();
      break;
    }
  }
  DCHECK(simple_menu_model);

  std::string error =
      PopulateModel(params_, menu_id, dark_text_color,
                    *id_to_elementvector_map_[menu_id], simple_menu_model);
  if (!error.empty()) {
    MenubarMenuAPI::SendError(browser_context(), error);
  }
}

// Chrome menu code will replace multiple separators with one, remove those
// at the start of a menu but not remove the last separator if it happens to be
// the last item in the menu. We want removal because automatic hiding of menu
// elements depending on state can easily make a separator the last item.
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
  Respond(ArgumentList(vivaldi::menubar_menu::Show::Results::Create()));
  Release();
}

void MenubarMenuShowFunction::OnAction(int command, int event_state) {
  MenubarMenuAPI::SendAction(browser_context(), command, event_state);
}

void MenubarMenuShowFunction::OnHover(const std::string& url) {
  MenubarMenuAPI::SendHover(browser_context(), params_->properties.window_id,
      url);
}

void MenubarMenuShowFunction::OnOpenBookmark(int64_t bookmark_id,
                                             int event_state) {
  MenubarMenuAPI::SendOpenBookmark(browser_context(),
      params_->properties.window_id, bookmark_id, event_state);
}

void MenubarMenuShowFunction::OnBookmarkAction(int64_t bookmark_id,
                                               int command) {
  MenubarMenuAPI::SendBookmarkAction(browser_context(),
      params_->properties.window_id, bookmark_id, command);
}

bool MenubarMenuShowFunction::IsBookmarkMenu(int menu_id) {
  return bookmark_menu_id_ == menu_id;
}

int MenubarMenuShowFunction::GetSelectedMenuId() {
  return selected_menu_id_;
}

bool MenubarMenuShowFunction::IsItemChecked(int id) {
  std::map<int, bool>::iterator it = id_to_checked_map_.find(id);
  return it != id_to_checked_map_.end() ? it->second : false;
}

bool MenubarMenuShowFunction::IsItemEnabled(int id) {
  // Note, we record the disabled entries as we normally have few disabled.
  std::map<int, bool>::iterator it = id_to_disabled_map_.find(id);
  return it != id_to_disabled_map_.end() ? !it->second : true;
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
