//
// Copyright (c) 2014-2015 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/show_menu/show_menu_api.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_load_service.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/core/favicon_service.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/vivaldi_context_menu.h"
#include "ui/vivaldi_main_menu.h"

namespace gfx {
class Rect;
}

namespace extensions {

// These symbols do not not exist in chrome or the definition differs
// from vivaldi.
const char* kVivaldiKeyEsc = "Esc";
const char* kVivaldiKeyDel = "Del";
const char* kVivaldiKeyIns = "Ins";
const char* kVivaldiKeyPgUp = "Pageup";
const char* kVivaldiKeyPgDn = "Pagedown";
const char* kVivaldiKeyMultiply = "*";
const char* kVivaldiKeyDivide = "/";
const char* kVivaldiKeySubtract = "-";
const char* kVivaldiKeyPeriod = ".";
const char* kVivaldiKeyComma = ",";

using content::BrowserContext;

namespace show_menu = vivaldi::show_menu;
namespace values = manifest_values;

namespace {
int TranslateCommandIdToMenuId(int command_id) {
  return command_id - IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST - 1;
}

const show_menu::MenuItem* GetMenuItemById(
    const std::vector<show_menu::MenuItem>* menuItems,
    int id) {
  for (std::vector<show_menu::MenuItem>::const_iterator it = menuItems->begin();
       it != menuItems->end(); ++it) {
    const show_menu::MenuItem& menuitem = *it;
    if (menuitem.id == id)
      return &menuitem;
    if (menuitem.items.get() && !menuitem.items->empty()) {
      const show_menu::MenuItem* item =
          GetMenuItemById(menuitem.items.get(), id);
      if (item) {
        return item;
      }
    }
  }
  return nullptr;
}

ui::KeyboardCode GetFunctionKey(std::string token) {
  int size = token.size();
  if (size >= 2 && token[0] == 'F') {
    if (size == 2 && token[1] >= '1' && token[1] <= '9')
      return static_cast<ui::KeyboardCode>(ui::VKEY_F1 + (token[1] - '1'));
    if (size == 3 && token[1] == '1' && token[2] >= '0' && token[2] <= '9')
      return static_cast<ui::KeyboardCode>(ui::VKEY_F10 + (token[2] - '0'));
    if (size == 3 && token[1] == '2' && token[2] >= '0' && token[2] <= '4')
      return static_cast<ui::KeyboardCode>(ui::VKEY_F20 + (token[2] - '0'));
  }
  return ui::VKEY_UNKNOWN;
}

// Based on extensions/command.cc
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
ui::Accelerator ParseShortcut(const std::string& accelerator,
                              bool should_parse_media_keys) {
  std::vector<std::string> tokens = base::SplitString(
      accelerator, "+", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() == 0) {
    return ui::Accelerator();
  }

  int modifiers = ui::EF_NONE;
  ui::KeyboardCode key = ui::VKEY_UNKNOWN;
  for (size_t i = 0; i < tokens.size(); i++) {
    if (tokens[i] == values::kKeyCtrl) {
      modifiers |= ui::EF_CONTROL_DOWN;
    } else if (tokens[i] == values::kKeyAlt) {
      modifiers |= ui::EF_ALT_DOWN;
    } else if (tokens[i] == values::kKeyShift) {
      modifiers |= ui::EF_SHIFT_DOWN;
    } else if (tokens[i] == values::kKeyCommand) {
      modifiers |= ui::EF_COMMAND_DOWN;
    } else if (key == ui::VKEY_UNKNOWN) {
      if (tokens[i] == values::kKeyUp) {
        key = ui::VKEY_UP;
      } else if (tokens[i] == values::kKeyDown) {
        key = ui::VKEY_DOWN;
      } else if (tokens[i] == values::kKeyLeft) {
        key = ui::VKEY_LEFT;
      } else if (tokens[i] == values::kKeyRight) {
        key = ui::VKEY_RIGHT;
      } else if (tokens[i] == values::kKeyIns) {
        key = ui::VKEY_INSERT;
      } else if (tokens[i] == values::kKeyDel) {
        key = ui::VKEY_DELETE;
      } else if (tokens[i] == values::kKeyHome) {
        key = ui::VKEY_HOME;
      } else if (tokens[i] == values::kKeyEnd) {
        key = ui::VKEY_END;
      } else if (tokens[i] == values::kKeyPgUp) {
        key = ui::VKEY_PRIOR;
      } else if (tokens[i] == values::kKeyPgDwn) {
        key = ui::VKEY_NEXT;
      } else if (tokens[i] == values::kKeySpace) {
        key = ui::VKEY_SPACE;
      } else if (tokens[i] == values::kKeyTab) {
        key = ui::VKEY_TAB;
      } else if (tokens[i] == kVivaldiKeyPeriod) {
        key = ui::VKEY_OEM_PERIOD;
      } else if (tokens[i] == kVivaldiKeyComma) {
        key = ui::VKEY_OEM_COMMA;
      } else if (tokens[i] == kVivaldiKeyEsc) {
        key = ui::VKEY_ESCAPE;
      } else if (tokens[i] == kVivaldiKeyDel) {
        key = ui::VKEY_DELETE;
      } else if (tokens[i] == kVivaldiKeyIns) {
        key = ui::VKEY_ESCAPE;
      } else if (tokens[i] == kVivaldiKeyPgUp) {
        key = ui::VKEY_PRIOR;
      } else if (tokens[i] == kVivaldiKeyPgDn) {
        key = ui::VKEY_NEXT;
      } else if (tokens[i] == kVivaldiKeyMultiply) {
        key = ui::VKEY_MULTIPLY;
      } else if (tokens[i] == kVivaldiKeyDivide) {
        key = ui::VKEY_DIVIDE;
      } else if (tokens[i] == kVivaldiKeySubtract) {
        key = ui::VKEY_SUBTRACT;
      } else if (tokens[i].size() == 0) {
        // Nasty workaround. The parser does not handle "++"
        key = ui::VKEY_ADD;
      } else if (tokens[i] == values::kKeyMediaNextTrack &&
                 should_parse_media_keys) {
        key = ui::VKEY_MEDIA_NEXT_TRACK;
      } else if (tokens[i] == values::kKeyMediaPlayPause &&
                 should_parse_media_keys) {
        key = ui::VKEY_MEDIA_PLAY_PAUSE;
      } else if (tokens[i] == values::kKeyMediaPrevTrack &&
                 should_parse_media_keys) {
        key = ui::VKEY_MEDIA_PREV_TRACK;
      } else if (tokens[i] == values::kKeyMediaStop &&
                 should_parse_media_keys) {
        key = ui::VKEY_MEDIA_STOP;
      } else if (tokens[i].size() == 1 && tokens[i][0] >= 'A' &&
                 tokens[i][0] <= 'Z') {
        key = static_cast<ui::KeyboardCode>(ui::VKEY_A + (tokens[i][0] - 'A'));
      } else if (tokens[i].size() == 1 && tokens[i][0] >= '0' &&
                 tokens[i][0] <= '9') {
        key = static_cast<ui::KeyboardCode>(ui::VKEY_0 + (tokens[i][0] - '0'));
      } else if ((key = GetFunctionKey(tokens[i])) != ui::VKEY_UNKNOWN) {
      } else {
        // Keep this for now.
        // printf("unknown key length=%lu, string=%s\n", tokens[i].size(),
        // tokens[i].c_str()); for(int j=0; j<(int)tokens[i].size(); j++) {
        //  printf("%02x ", tokens[i][j]);
        //}
        // printf("\n");
      }
    }
  }
  if (key == ui::VKEY_UNKNOWN) {
    return ui::Accelerator();
  } else {
    return ui::Accelerator(key, modifiers);
  }
}

}  // namespace

CommandEventRouter::CommandEventRouter(Profile* profile)
    : browser_context_(profile) {}

CommandEventRouter::~CommandEventRouter() {}

void CommandEventRouter::DispatchEvent(
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->BroadcastEvent(base::WrapUnique(
        new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                              event_name, std::move(event_args))));
  }
}

ShowMenuAPI::ShowMenuAPI(BrowserContext* context) : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this,
                                 show_menu::OnMainMenuCommand::kEventName);
  event_router->RegisterObserver(this, show_menu::OnOpen::kEventName);
  event_router->RegisterObserver(this, show_menu::OnUrlHighlighted::kEventName);
}

ShowMenuAPI::~ShowMenuAPI() {}

void ShowMenuAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void ShowMenuAPI::CommandExecuted(int command_id,
                                  const std::string& parameter) {
  if (command_event_router_) {
    command_event_router_->DispatchEvent(
        show_menu::OnMainMenuCommand::kEventName,
        show_menu::OnMainMenuCommand::Create(command_id, parameter));
  }
}

void ShowMenuAPI::OnOpen() {
  if (command_event_router_) {
    command_event_router_->DispatchEvent(show_menu::OnOpen::kEventName,
                                         show_menu::OnOpen::Create());
  }
}

void ShowMenuAPI::OnUrlHighlighted(const std::string& url) {
  if (command_event_router_) {
    command_event_router_->DispatchEvent(
        show_menu::OnUrlHighlighted::kEventName,
        show_menu::OnUrlHighlighted::Create(url));
  }
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<ShowMenuAPI>>::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ShowMenuAPI>* ShowMenuAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void ShowMenuAPI::OnListenerAdded(const EventListenerInfo& details) {
  command_event_router_.reset(
      new CommandEventRouter(Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

VivaldiMenuController::VivaldiMenuController(
    Delegate* delegate,
    std::vector<show_menu::MenuItem>* menu_items)
    : favicon_service_(nullptr),
      delegate_(delegate),
      menu_items_(menu_items),
      menu_model_(this),
      web_contents_(nullptr),
      profile_(nullptr),
      current_highlighted_id_(-1),
      is_url_highlighted_(false),
      initial_selected_id_(-1),
      is_shown_(false) {}

VivaldiMenuController::~VivaldiMenuController() {}

void VivaldiMenuController::Show(
    content::WebContents* web_contents,
    const content::ContextMenuParams& menu_params) {
  web_contents_ = web_contents;
  menu_params_ = menu_params;

  content::BrowserContext* browser_context = web_contents_->GetBrowserContext();
  profile_ = Profile::FromBrowserContext(browser_context);

  // Populate menu
  for (std::vector<show_menu::MenuItem>::const_iterator it =
           menu_items_->begin();
       it != menu_items_->end(); ++it) {
    const show_menu::MenuItem& menuitem = *it;
    PopulateModel(&menuitem, &menu_model_);
  }

  if (HasDeveloperTools()) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP,
                                    IDS_CONTENT_CONTEXT_RELOAD_PACKAGED_APP);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP,
                                    IDS_CONTENT_CONTEXT_RESTART_APP);
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_INSPECTELEMENT,
                                    IDS_CONTENT_CONTEXT_INSPECTELEMENT);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE,
                                    IDS_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE);
  }

  SanitizeModel(&menu_model_);

  menu_.reset(::vivaldi::CreateVivaldiContextMenu(
      web_contents->GetFocusedFrame(), &menu_model_, menu_params));
  ShowMenuAPI::GetFactoryInstance()->Get(profile_)->OnOpen();
  menu_->Show();
}

void VivaldiMenuController::MenuWillShow(ui::SimpleMenuModel* source) {
  if (!is_shown_) {
    is_shown_ = true;
    if (source == &menu_model_ && initial_selected_id_ != -1) {
      menu_->SetSelectedItem(initial_selected_id_);
    }
  }
}

void VivaldiMenuController::MenuClosed(ui::SimpleMenuModel* source) {
  source->SetMenuModelDelegate(nullptr);
  if (source == &menu_model_) {
    if (delegate_) {
      delegate_->OnMenuCanceled();
    }
    delete this;
  }
}

bool VivaldiMenuController::HasDeveloperTools() {
  if (!web_contents_) {
    return false;
  } else {
    const extensions::Extension* platform_app = GetExtension();
    return extensions::Manifest::IsUnpackedLocation(platform_app->location()) ||
           base::CommandLine::ForCurrentProcess()->HasSwitch(
               switches::kDebugPackedApps);
  }
}

bool VivaldiMenuController::IsDeveloperTools(int command_id) const {
  return command_id == IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP ||
         command_id == IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP ||
         command_id == IDC_CONTENT_CONTEXT_INSPECTELEMENT ||
         command_id == IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE;
}

void VivaldiMenuController::HandleDeveloperToolsCommand(int command_id) {
  const extensions::Extension* platform_app = GetExtension();
  content::BrowserContext* browser_context = web_contents_->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);

  switch (command_id) {
    case IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP:
      if (platform_app && platform_app->is_platform_app()) {
        extensions::ExtensionSystem::Get(browser_context)
            ->extension_service()
            ->ReloadExtension(platform_app->id());
      }
      break;

    case IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP:
      if (platform_app && platform_app->is_platform_app()) {
        apps::AppLoadService::Get(profile)->RestartApplication(
            platform_app->id());
      }
      break;

    case IDC_CONTENT_CONTEXT_INSPECTELEMENT: {
      DevToolsWindow::InspectElement(web_contents_->GetMainFrame(),
                                     menu_params_.x, menu_params_.y);
    } break;

    case IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE:
      if (platform_app && platform_app->is_platform_app()) {
        extensions::devtools_util::InspectBackgroundPage(platform_app, profile);
      }
      break;
  }
}

const Extension* VivaldiMenuController::GetExtension() const {
  if (!web_contents_) {
    return nullptr;
  } else {
    ProcessManager* process_manager =
        ProcessManager::Get(web_contents_->GetBrowserContext());
    return process_manager->GetExtensionForWebContents(web_contents_);
  }
}

void VivaldiMenuController::PopulateModel(const show_menu::MenuItem* item,
                                          ui::SimpleMenuModel* menu_model) {
  // Offset the command ids into the range of extension custom commands
  // plus add one to allow -1 as a command id.
  int id = item->id + IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST + 1;
  const base::string16 label = base::UTF8ToUTF16(item->name);

  if (item->name.find("---") == 0) {
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  } else {
    if (initial_selected_id_ == -1 && item->selected && *item->selected) {
      initial_selected_id_ = id;
    }
    bool load_image = false;
    bool visible = item->visible ? *item->visible : true;
    if (visible) {
      if (item->items) {
        ui::SimpleMenuModel* child_menu_model = new ui::SimpleMenuModel(this);
        models_.push_back(child_menu_model);
        for (std::vector<show_menu::MenuItem>::const_iterator it =
                 item->items->begin();
             it != item->items->end(); ++it) {
          const show_menu::MenuItem& child = *it;
          PopulateModel(&child, child_menu_model);
        }
        SanitizeModel(child_menu_model);
        menu_model->AddSubMenu(id, label, child_menu_model);
        load_image = true;
      } else {
        if (item->type.get() && item->type->compare("checkbox") == 0) {
          menu_model->AddCheckItem(id, label);
        } else {
          menu_model->AddItem(id, label);
          load_image = true;
        }
      }
    }
    if (load_image) {
      if (item->icon.get() && item->icon->length() > 0) {
        std::string png_data;
        if (base::Base64Decode(*item->icon, &png_data)) {
          gfx::Image img = gfx::Image::CreateFrom1xPNGBytes(
              reinterpret_cast<const unsigned char*>(png_data.c_str()),
              png_data.length());
          menu_model->SetIcon(menu_model->GetIndexOfCommandId(id), img);
        }
      }
      if (item->url.get() && item->url->length() > 0) {
        url_map_[id] = item->url.get();
        LoadFavicon(id, *item->url);
      }
    }
  }
}

void VivaldiMenuController::SanitizeModel(ui::SimpleMenuModel* menu_model) {
  for (int i = menu_model->GetItemCount() - 1; i >= 0; i--) {
    if (menu_model->GetTypeAt(i) == ui::MenuModel::TYPE_SEPARATOR) {
      menu_model->RemoveItemAt(i);
    } else {
      break;
    }
  }
}

void VivaldiMenuController::LoadFavicon(int command_id,
                                        const std::string& url) {
  if (!favicon_service_) {
    favicon_service_ = FaviconServiceFactory::GetForProfile(
        profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (!favicon_service_)
      return;
  }

  favicon_base::FaviconImageCallback callback =
      base::Bind(&VivaldiMenuController::OnFaviconDataAvailable,
                 base::Unretained(this), command_id);

  favicon_service_->GetFaviconImageForPageURL(GURL(url), callback,
                                              &cancelable_task_tracker_);
}

void VivaldiMenuController::OnFaviconDataAvailable(
    int command_id,
    const favicon_base::FaviconImageResult& image_result) {
  if (!image_result.image.IsEmpty()) {
    // We do not update the model. The MenuItemView class we use to paint the
    // menu does not support dynamic updates of icons through the model. We have
    // to set it directly.
    menu_->SetIcon(image_result.image, command_id);
  }
}

bool VivaldiMenuController::IsCommandIdChecked(int command_id) const {
  const show_menu::MenuItem* item = getItemByCommandId(command_id);
  return item && item->checked.get() && *item->checked;
}

bool VivaldiMenuController::IsCommandIdEnabled(int command_id) const {
  const show_menu::MenuItem* item = getItemByCommandId(command_id);
  return item != nullptr || IsDeveloperTools(command_id);
}

bool VivaldiMenuController::IsItemForCommandIdDynamic(int command_id) const {
  return false;
}

base::string16 VivaldiMenuController::GetLabelForCommandId(
    int command_id) const {
  const show_menu::MenuItem* item = getItemByCommandId(command_id);
  return base::UTF8ToUTF16(item ? item->name : std::string());
}

bool VivaldiMenuController::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  const show_menu::MenuItem* item = getItemByCommandId(command_id);
  if (item && item->shortcut) {
    // printf("shortcut: %s\n", item->shortcut->c_str());
    *accelerator = ParseShortcut(*item->shortcut, true);
    return true;
  } else if (IsDeveloperTools(command_id)) {
    switch (command_id) {
      case IDC_CONTENT_CONTEXT_INSPECTELEMENT:
        *accelerator = ui::Accelerator(ui::VKEY_I,
                                       ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
        return true;
        break;
    }
  }

  return false;
}

bool VivaldiMenuController::GetIconForCommandId(int command_id,
                                                gfx::Image* icon) const {
  return false;
}

void VivaldiMenuController::CommandIdHighlighted(int command_id) {
  if (current_highlighted_id_ != command_id) {
    current_highlighted_id_ = command_id;
    std::map<int, std::string*>::iterator it = url_map_.find(command_id);
    if (it != url_map_.end()) {
      ShowMenuAPI::GetFactoryInstance()->Get(profile_)->OnUrlHighlighted(
          *it->second);
      is_url_highlighted_ = true;
    } else if (is_url_highlighted_) {
      is_url_highlighted_ = false;
      ShowMenuAPI::GetFactoryInstance()->Get(profile_)->OnUrlHighlighted("");
    }
  }
}

void VivaldiMenuController::ExecuteCommand(int command_id, int event_flags) {
  if (IsDeveloperTools(command_id)) {
    // These are the commands we only get when running with npm.
    // For JS, this menu has been canceled since we handle the actions here.
    HandleDeveloperToolsCommand(command_id);
    delegate_->OnMenuCanceled();
  } else {
    delegate_->OnMenuActivated(command_id, event_flags);
  }
  delegate_ = nullptr;
}

const show_menu::MenuItem* VivaldiMenuController::getItemByCommandId(
    int command_id) const {
  return GetMenuItemById(menu_items_, TranslateCommandIdToMenuId(command_id));
}

namespace Create = vivaldi::show_menu::Create;

bool ShowMenuCreateFunction::RunAsync() {
  params_ = Create::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  if (params_->create_properties.mode == "context") {
    content::ContextMenuParams menu_params;
    menu_params.x = params_->create_properties.left;
    menu_params.y = params_->create_properties.top;

    // Balanced in OnMenuActivated / OnMenuClosed
    AddRef();
    // Menu is deallocated when main menu model is closed.
    VivaldiMenuController* menu =
        new VivaldiMenuController(this, &params_->create_properties.items);
    menu->Show(GetAssociatedWebContents(), menu_params);
#if defined(OS_MACOSX)
  } else {
    // Mac needs to update the menu even with no open windows. So we allow
    // a nullptr profile when calling the api.
    Profile* profile = nullptr;
    if (GetAssociatedWebContents()) {
      content::BrowserContext* browser_context =
          GetAssociatedWebContents()->GetBrowserContext();
      profile = Profile::FromBrowserContext(browser_context);
    }
    ::vivaldi::CreateVivaldiMainMenu(profile, &params_->create_properties.items,
                                     params_->create_properties.mode);
#endif
  }
  AddRef();
  return true;
}

void ShowMenuCreateFunction::OnMenuActivated(int command_id, int event_flags) {
  int id = TranslateCommandIdToMenuId(command_id);

  show_menu::Response response;
  response.id = id;
  response.ctrl = event_flags & ui::EF_CONTROL_DOWN ? true : false;
  response.shift = event_flags & ui::EF_SHIFT_DOWN ? true : false;
  response.alt = event_flags & ui::EF_ALT_DOWN ? true : false;
  response.command = event_flags & ui::EF_COMMAND_DOWN ? true : false;
  response.left = event_flags & ui::EF_LEFT_MOUSE_BUTTON ? true : false;
  response.right = event_flags & ui::EF_RIGHT_MOUSE_BUTTON ? true : false;
  response.center = event_flags & ui::EF_MIDDLE_MOUSE_BUTTON ? true : false;
  Respond(ArgumentList(vivaldi::show_menu::Create::Results::Create(response)));
  Release();
}

void ShowMenuCreateFunction::OnMenuCanceled() {
  show_menu::Response response;
  response.id = -1;
  response.ctrl = response.shift = response.alt = response.command = false;
  response.left = response.right = response.center = false;
  Respond(ArgumentList(vivaldi::show_menu::Create::Results::Create(response)));
  Release();
}

ShowMenuCreateFunction::ShowMenuCreateFunction() {
}

ShowMenuCreateFunction::~ShowMenuCreateFunction() {
}

}  // namespace extensions
