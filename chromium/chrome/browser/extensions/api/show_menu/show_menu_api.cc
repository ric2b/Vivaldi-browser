//
// Copyright (c) 2014-2015 Vivaldi Technologies AS. All rights reserved.
//

#include "chrome/browser/extensions/api/show_menu/show_menu_api.h"

#include <string>

#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "extensions/browser/event_router.h"

namespace gfx {
class Rect;
}

namespace extensions {

using content::BrowserContext;

namespace show_menu = api::show_menu;

namespace {
int TranslateCommandIdToMenuId(int command_id) {
  return command_id - IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST - 1;
}
}

CommandEventRouter::CommandEventRouter(Profile* profile)
    : browser_context_(profile) {
}

CommandEventRouter::~CommandEventRouter() {
}

void CommandEventRouter::DispatchEvent(const std::string& event_name,
                                       scoped_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->BroadcastEvent(make_scoped_ptr(new extensions::Event(
        extensions::events::UNKNOWN, event_name, event_args.Pass())));
  }
}

void CommandEventRouter::CommandExecuted(int command_id) {
  DispatchEvent(show_menu::OnMainMenuCommand::kEventName,
                show_menu::OnMainMenuCommand::Create(command_id));
}

ShowMenuAPI::ShowMenuAPI(BrowserContext* context) : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this,
                                 show_menu::OnMainMenuCommand::kEventName);
}

ShowMenuAPI::~ShowMenuAPI() {
}

void ShowMenuAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void ShowMenuAPI::CommandExecuted(int command_id) {
  if (command_event_router_.get())
    command_event_router_->CommandExecuted(command_id);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<ShowMenuAPI>>
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ShowMenuAPI>* ShowMenuAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void ShowMenuAPI::OnListenerAdded(const EventListenerInfo& details) {
  command_event_router_.reset(
      new CommandEventRouter(Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

VivaldiMenuObserver::VivaldiMenuObserver(
    RenderViewContextMenuBase* proxy,
    std::vector<linked_ptr<api::show_menu::MenuItem>>* menu_items,
    ShowMenuCreateFunction* ptr)
    : proxy_(proxy), menu_items_(menu_items), menuCreateCallback_(ptr) {
  DCHECK(proxy_);
}

VivaldiMenuObserver::~VivaldiMenuObserver() {
}

void VivaldiMenuObserver::InitMenu(const content::ContextMenuParams& params) {
  for (std::vector<linked_ptr<api::show_menu::MenuItem>>::const_iterator it =
           menu_items_->begin();
       it != menu_items_->end(); ++it) {
    const api::show_menu::MenuItem& menuitem = **it;
    AddMenuHelper(&menuitem, NULL);
  }
}

bool VivaldiMenuObserver::IsCommandIdSupported(int command_id) {
  // TODO:  Could check if id in menu_items_ .
  if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    return true;
  }
  return false;
}

bool VivaldiMenuObserver::IsCommandIdEnabled(int command_id) {
  // TODO:  Could check if id in menu_items_ .
  if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    return true;
  }
  return false;
}

namespace {
bool IsCommandIdCheckedInternal(
    const std::vector<linked_ptr<api::show_menu::MenuItem>>* menuItems,
    int id) {
  for (std::vector<linked_ptr<api::show_menu::MenuItem>>::const_iterator it =
           menuItems->begin();
       it != menuItems->end(); ++it) {
    const api::show_menu::MenuItem& menuitem = **it;
    if (menuitem.id == id && menuitem.checked.get() && *menuitem.checked) {
      return true;
    }
    if (menuitem.items.get() && !menuitem.items->empty()) {
      if (IsCommandIdCheckedInternal(menuitem.items.get(), id)) {
        return true;
      }
    }
  }
  return false;
}
}

bool VivaldiMenuObserver::IsCommandIdChecked(int command_id) {
  // Traverse all items recursively.
  return IsCommandIdCheckedInternal(menu_items_,
                                    TranslateCommandIdToMenuId(command_id));
}

void VivaldiMenuObserver::ExecuteCommand(int command_id) {
  // TODO:  Could check if id in menu_items_ .
  if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    menuCreateCallback_->menuSelected(command_id);
  }
}

void VivaldiMenuObserver::OnMenuCancel() {
}

// Maybe the menu should not be built in the menu observer
// or this should not be called MenuObserver
void VivaldiMenuObserver::AddMenuHelper(
    const api::show_menu::MenuItem* menuitem,
    ui::SimpleMenuModel* sub_menu_model) {
  // Offset the command ids into the range of extension custom commands
  // plus add one to allow -1 as a command id.
  int id = menuitem->id + IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST + 1;
  const base::string16 label = base::UTF8ToUTF16(menuitem->name);

  if (menuitem->name.find("---") == 0) {
    if (sub_menu_model) {
      sub_menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    } else {
      proxy_->AddSeparator();
    }
  } else if (menuitem->items) {
    ui::SimpleMenuModel* menu_model = new ui::SimpleMenuModel(proxy_);
    for (std::vector<linked_ptr<api::show_menu::MenuItem>>::const_iterator it =
             menuitem->items->begin();
         it != menuitem->items->end(); ++it) {
      const api::show_menu::MenuItem& submenuitem = **it;
      AddMenuHelper(&submenuitem, menu_model);
    }
    if (sub_menu_model) {
      sub_menu_model->AddSubMenu(id, label, menu_model);
    } else {
      proxy_->AddSubMenu(id, label, menu_model);
    }
  } else {
    bool visible = menuitem->visible ? *menuitem->visible : true;
    if (visible) {
      if (sub_menu_model) {
        if (menuitem->type.get() && menuitem->type->compare("checkbox") == 0) {
          sub_menu_model->AddCheckItem(id, label);
        } else {
          sub_menu_model->AddItem(id, label);
        }
      } else {
        if (menuitem->type.get() && menuitem->type->compare("checkbox") == 0) {
          proxy_->AddCheckItem(id, label);
        } else {
          proxy_->AddMenuItem(id, label);
        }
      }
    }
  }
}

namespace Create = api::show_menu::Create;

bool ShowMenuCreateFunction::RunAsync() {
  scoped_ptr<Create::Params> params(Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  gfx::Point menu_position(params->create_properties.left,
                           params->create_properties.top);

  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(GetAssociatedWebContents());

  content::ContextMenuParams menu_params;
  menu_params.x = params->create_properties.left;
  menu_params.y = params->create_properties.top;

  scoped_ptr<RenderViewContextMenuBase> menu;
  menu = menu_delegate->BuildMenu(GetAssociatedWebContents(), menu_params);

  VivaldiMenuObserver* mo = new VivaldiMenuObserver(
      menu.get(), &params->create_properties.items, this);
  menu->AddVivaldiMenu(mo);
  menu_delegate->ShowMenu(menu.Pass());

  return true;
}

void ShowMenuCreateFunction::menuSelected(int command_id) {
  int id = TranslateCommandIdToMenuId(command_id);
  SetResult(new base::FundamentalValue(static_cast<int>(id)));
  SendResponse(true);
  menu_cancelled_ = false;
}

ShowMenuCreateFunction::ShowMenuCreateFunction() :
 menu_cancelled_(true) {
}

ShowMenuCreateFunction::~ShowMenuCreateFunction() {
  if (menu_cancelled_) {
    SetResult(new base::FundamentalValue(static_cast<int>(-1)));
    SendResponse(true);
  }
}

}  // namespace extensions
