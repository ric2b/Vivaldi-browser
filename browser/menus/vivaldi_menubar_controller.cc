//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#include "browser/menus/vivaldi_menubar_controller.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "browser/menus/vivaldi_bookmark_context_menu.h"
#include "browser/menus/vivaldi_menu_enums.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/favicon/core/favicon_service.h"
#include "components/prefs/pref_service.h"
#include "extensions/api/menubar_menu/menubar_menu_api.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/widget.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_context_menu.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

static bool IsBookmarkCommand(int command_id) {
  return command_id >= IDC_FIRST_UNBOUNDED_MENU;
}

SkColor TextColorForMenu(views::MenuItemView* menu, views::Widget* widget) {
  // Use the same code as in MenuItemView::GetTextColor() for best result.
  if (widget && widget->GetNativeTheme()) {
    return widget->GetColorProvider()->GetColor(ui::kColorMenuItemForeground);
  } else {
    return menu->GetColorProvider()->GetColor(ui::kColorMenuItemForeground);
  }
}

std::unique_ptr<MenubarController> MenubarController::active_controller_;

// This is the maximum id we can assing a menu element starting from 0 in JS
// when setting up a menu. We add IDC_VIV_MENU_FIRST to ids from JS here in the
// controller when populating.
// static
int MenubarController::GetMaximumId() {
  return IDC_FIRST_UNBOUNDED_MENU - IDC_VIV_MENU_FIRST - 1;
}

// static
MenubarController* MenubarController::Create(
    VivaldiBrowserWindow* browser_window,
    std::optional<Params> params) {
  active_controller_.reset(new MenubarController(
    browser_window, std::move(params)));
  return active_controller_.get();
}

// The Menubar controller is a support class for the MenubarMenu api. The api
// handles both horizontal and vertical (vivaldi menu) modes. Menus are created
// on demand just before shown to mimimize impact of large menus.
MenubarController::MenubarController(
  VivaldiBrowserWindow* browser_window,
  std::optional<Params> params)
    : browser_window_(browser_window),
      web_contents_(browser_window->web_contents()),
      browser_(FindBrowserForEmbedderWebContents(
          browser_window->web_contents())),
      params_(std::move(params)),
      run_types_(views::MenuRunner::SHOULD_SHOW_MNEMONICS) {

  browser_window_->GetWidget()->AddObserver(this);

  state_.siblings.reserve(params_->properties.siblings.size());
  for (const extensions::vivaldi::menubar_menu::Menu& m :
       params_->properties.siblings) {
    state_.siblings.emplace_back();
    ::vivaldi::MenubarMenuEntry* entry = &state_.siblings.back();
    entry->id = m.id;
    entry->rect = gfx::Rect(m.rect.x, m.rect.y, m.rect.width, m.rect.height);
  }
  ConvertMenubarButtonRectToScreen(web_contents_, state_);

  SetActiveMenu(params->properties.id);
}

MenubarController::~MenubarController() {
  // Ensure that all top level items (this is only important for menu bar mode
  // with multiple top level items - vivaldi menu mode with one top level item
  // is already handled in chrome code) have no delegate anymore (this).
  std::map<int, views::MenuItemView*>::iterator it;
  for (it = id_to_menu_map_.begin(); it != id_to_menu_map_.end(); ++it)
    it->second->set_delegate(nullptr);

  // Will be null if browser window has been destroyed.
  if (browser_window_) {
    browser_window_->GetWidget()->RemoveObserver(this);
  }
}

// Called when browser window is being destroyed.
void MenubarController::OnWidgetDestroying(views::Widget* widget) {
  browser_window_->GetWidget()->RemoveObserver(this);
  browser_window_ = nullptr;
  active_controller_.reset(nullptr);
}

Profile* MenubarController::GetProfile() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

void MenubarController::SetActiveMenu(int id) {
  active_menu_id_ = id;
  std::map<int, views::MenuItemView*>::iterator it =
      id_to_menu_map_.find(active_menu_id_);
  if (it == id_to_menu_map_.end()) {
    Populate(active_menu_id_);
  }
  extensions::MenubarMenuAPI::SendOpen(GetProfile(), id);
}

bool MenubarController::IsDarkTextColor(views::MenuItemView* menu) {
  views::Widget* parent = views::Widget::GetWidgetForNativeWindow(
      browser_->window()->GetNativeWindow());
  return color_utils::IsDark(TextColorForMenu(menu, parent));
}

// Populates the toplevel of a menu (vertical), or the specified menu (eg File)
// of a horizonal. Sub menu or other menus of the horizonal are created on
// demand in WillShowMenu()
void MenubarController::Populate(int id) {
  views::MenuItemView* root = new views::MenuItemView(this);
  id_to_menu_map_[id] = root;
  ui::MenuModel* menu_model = nullptr;
  PopulateModel(id, IsDarkTextColor(root), &menu_model);
  DCHECK(menu_model);
  PopulateMenu(root, menu_model);

  if (IsBookmarkMenu(id)) {
    bookmark_menu_ = root;
  }
}

// Called by menu code to populate the top level of a menu model
void MenubarController::PopulateModel(int menu_id,
                                      bool dark_text_color,
                                      ui::MenuModel** menu_model) {
  using extensions::vivaldi::menubar_menu::Menu;
  using extensions::vivaldi::menubar_menu::Show::Params;

  ui::SimpleMenuModel* simple_menu_model = new ui::SimpleMenuModel(nullptr);
  models_.push_back(base::WrapUnique(simple_menu_model));  // We own the model.
  *menu_model = simple_menu_model;

  std::vector<Menu>& list = params_->properties.siblings;
  for (const Menu& sibling : list) {
    if (sibling.id == menu_id) {
      std::string error =
          PopulateModel(sibling.id, dark_text_color,
                        sibling.children, simple_menu_model);
      if (!error.empty()) {
        extensions::MenubarMenuAPI::SendError(GetProfile(), error);
      }
      break;
    }
  }
}

// Called by menu code to populate a sub menu model of an existing menu model
void MenubarController::PopulateSubmodel(int menu_id,
                                         bool dark_text_color,
                                         ui::MenuModel* menu_model) {
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
      PopulateModel(menu_id, dark_text_color,
                    *id_to_elementvector_map_[menu_id], simple_menu_model);
  if (!error.empty()) {
    extensions::MenubarMenuAPI::SendError(GetProfile(), error);
  }
}


std::string MenubarController::PopulateModel(int menu_id,
    bool dark_text_color,
    const std::vector<Element>& list,
    ui::SimpleMenuModel* menu_model) {
  bool prev_is_bookmarks = false;
  namespace menubar_menu = extensions::vivaldi::menubar_menu;
  for (const Element& child : list) {
    if (child.item) {
      prev_is_bookmarks = false;
      const Item& item = *child.item;
      int id = item.id + IDC_VIV_MENU_FIRST;
      const std::u16string label = base::UTF8ToUTF16(item.name);
      switch (item.type) {
        case menubar_menu::ItemType::kCommand:
          menu_model->AddItem(id, label);
          if (item.enabled && !*item.enabled) {
            id_to_disabled_map_[id] = true;
          }
          break;
        case menubar_menu::ItemType::kCheckbox:
          menu_model->AddCheckItem(id, label);
          id_to_checked_map_[id] = item.checked && *item.checked;
          if (item.enabled && !*item.enabled) {
            id_to_disabled_map_[id] = true;
          }
          break;
        case menubar_menu::ItemType::kRadio:
          if (!item.radiogroup.has_value()) {
            return "Radio button added without group";
          }
          menu_model->AddRadioItem(id, label, item.radiogroup.value());
          id_to_checked_map_[id] = item.checked.value_or(false);
          if (item.enabled && !*item.enabled) {
            id_to_disabled_map_[id] = true;
          }
          break;
        case menubar_menu::ItemType::kFolder: {
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
        case menubar_menu::ItemType::kNone:
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
              gfx::Image img = gfx::Image::CreateFrom1xPNGBytes(base::span(
                  reinterpret_cast<const unsigned char*>(png_data.c_str()),
                  png_data.length()));
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
      if (child.container->type == menubar_menu::ContainerType::kBookmarks) {
        if (bookmark_menu_container_) {
          return "Only one bookmark container supported";
        }
        prev_is_bookmarks = true;
        bookmark_menu_id_ = menu_id;
        bookmark_menu_container_.reset(
            new ::vivaldi::BookmarkMenuContainer(this));
        switch (child.container->edge) {
          case menubar_menu::Edge::kAbove:
            bookmark_menu_container_->edge =
                ::vivaldi::BookmarkMenuContainer::Above;
            break;
          case menubar_menu::Edge::kBelow:
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
        bookmark_menu_container_->support.initIcons(params_->properties.icons);
        switch (child.container->sort_field) {
          case menubar_menu::SortField::kNone:
            bookmark_menu_container_->sort_field =
                ::vivaldi::BookmarkSorter::FIELD_NONE;
            break;
          case menubar_menu::SortField::kTitle:
            bookmark_menu_container_->sort_field =
                ::vivaldi::BookmarkSorter::FIELD_TITLE;
            break;
          case menubar_menu::SortField::kUrl:
            bookmark_menu_container_->sort_field =
                ::vivaldi::BookmarkSorter::FIELD_URL;
            break;
          case menubar_menu::SortField::kNickname:
            bookmark_menu_container_->sort_field =
                ::vivaldi::BookmarkSorter::FIELD_NICKNAME;
            break;
          case menubar_menu::SortField::kDescription:
            bookmark_menu_container_->sort_field =
                ::vivaldi::BookmarkSorter::FIELD_NICKNAME;
            break;
          case menubar_menu::SortField::kDateAdded:
            bookmark_menu_container_->sort_field =
                ::vivaldi::BookmarkSorter::FIELD_DATEADDED;
            break;
        };
        switch (child.container->sort_order) {
          case menubar_menu::SortOrder::kNone:
            bookmark_menu_container_->sort_order =
                ::vivaldi::BookmarkSorter::ORDER_NONE;
            break;
          case menubar_menu::SortOrder::kAscending:
            bookmark_menu_container_->sort_order =
                ::vivaldi::BookmarkSorter::ORDER_ASCENDING;
            break;
          case menubar_menu::SortOrder::kDescending:
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

// Chrome menu code will replace multiple separators with one, remove those
// at the start of a menu but not remove the last separator if it happens to be
// the last item in the menu. We want removal because automatic hiding of menu
// elements depending on state can easily make a separator the last item.
void MenubarController::SanitizeModel(ui::SimpleMenuModel* menu_model) {
  for (int i = menu_model->GetItemCount() - 1; i >= 0; i--) {
    if (menu_model->GetTypeAt(i) == ui::MenuModel::TYPE_SEPARATOR) {
      menu_model->RemoveItemAt(i);
    } else {
      break;
    }
  }
}

void MenubarController::PopulateBookmarks() {
  if (bookmark_menu_delegate_) {
    return;
  }

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  if (!model->loaded()) {
    return;
  }

  views::Widget* parent = views::Widget::GetWidgetForNativeWindow(
      browser_->window()->GetNativeWindow());
  bookmark_menu_delegate_.reset(new BookmarkMenuDelegate(
      browser_,
      parent));
  bookmark_menu_delegate_->Init(this, bookmark_menu_,
                                model->bookmark_bar_node(), 0,
                                BookmarkMenuDelegate::HIDE_PERMANENT_FOLDERS,
                                BookmarkLaunchLocation::kNone);
}

void MenubarController::PopulateMenu(views::MenuItemView* parent,
                                     ui::MenuModel* model) {
  for (int i = 0, max = model->GetItemCount(); i < max; ++i) {
    // Add the menu item at the end.
    int menu_index =
        parent->HasSubmenu()
            ? static_cast<int>(parent->GetSubmenu()->children().size())
            : 0;
    AddMenuItem(parent, menu_index, model, i, model->GetTypeAt(i));

    if (model->GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU) {
      id_to_menumodel_map_[model->GetCommandIdAt(i)] =
          model->GetSubmenuModelAt(i);
    }
  }
}

views::MenuItemView* MenubarController::AddMenuItem(views::MenuItemView* parent,
                                          int menu_index,
                                          ui::MenuModel* model,
                                          int model_index,
                                          ui::MenuModel::ItemType menu_type) {
  int command_id = model->GetCommandIdAt(model_index);
  views::MenuItemView* menu_item =
      views::MenuModelAdapter::AddMenuItemFromModelAt(
          model, model_index, parent, menu_index, command_id);

  if (menu_item) {
    if (model->GetTypeAt(model_index) == ui::MenuModel::TYPE_COMMAND) {
      std::string url;
      if (GetUrl(command_id, &url)) {
        RequestFavicon(command_id, active_menu_id_, url);
      }
    }
  }

  return menu_item;
}

void MenubarController::Show() {
  views::Widget* parent = views::Widget::GetWidgetForNativeWindow(
    browser_->window()->GetNativeWindow());
  if (!parent) {
    extensions::MenubarMenuAPI::SendError(GetProfile(), "No parent");
    extensions::MenubarMenuAPI::SendClose(GetProfile());
  } else if (active_menu_id_ < 0) {
    extensions::MenubarMenuAPI::SendError(GetProfile(), "No menu");
    extensions::MenubarMenuAPI::SendClose(GetProfile());
  } else {
    Profile* profile = Profile::FromBrowserContext(
        web_contents_->GetBrowserContext());
    views::MenuController::VivaldiSetCompactLayout(
        profile->GetPrefs()->GetBoolean(vivaldiprefs::kMenuCompact));
    views::MenuController::VivaldiSetContextMenu(false);

    int32_t types = views::MenuRunner::HAS_MNEMONICS;
    if (run_types_ & views::MenuRunner::SHOULD_SHOW_MNEMONICS)
      types |= views::MenuRunner::SHOULD_SHOW_MNEMONICS;
    // The root menus (the one we create here and its siblings) will be managed
    // by the menu runner and released when menu runner terminates.
    std::unique_ptr<views::MenuItemView> root(id_to_menu_map_[active_menu_id_]);
    menu_runner_.reset(new views::MenuRunner(std::move(root), types));

    const gfx::Rect& rect = state_.GetSibling(active_menu_id_)->rect;
    menu_runner_->RunMenuAt(parent, nullptr, rect,
                            views::MenuAnchorPosition::kTopLeft,
                            ui::mojom::MenuSourceType::kNone);
  }
}

void MenubarController::WillShowMenu(views::MenuItemView* menu) {
  int id = menu->GetCommand();
  ui::MenuModel* menu_model = id_to_menumodel_map_[id];
  if (menu_model && menu_model->GetItemCount() == 0) {
    PopulateSubmodel(id, IsDarkTextColor(menu), menu_model);
    PopulateMenu(menu, id_to_menumodel_map_[id]);
    if (IsBookmarkMenu(id)) {
      bookmark_menu_ = menu;
    }
  }

  if (has_been_shown_ == false) {
    has_been_shown_ = true;
    // When using this class for the Vivaldi menu we support shortcuts opening
    // a sub menu as a child of the Vivaldi menu once the latter opens. This can
    // only happen one time, that is, when the Vivaldi menu opens.
    views::MenuItemView* item = menu->GetMenuItemByID(GetSelectedMenuId());
    if (item) {
      views::MenuController::GetActiveInstance()->VivaldiOpenMenu(item);
    }
  }

  if (menu == bookmark_menu_) {
    // Top level
    vivaldi::BookmarkMenuContainer* container = bookmark_menu_container_.get();
    if (container) {
      vivaldi::SetBookmarkContainer(container, 0);
      PopulateBookmarks();
    }

    // Top level
    PopulateBookmarks();
  } else if (bookmark_menu_delegate_) {
    // Bookmark sub menu
    bookmark_menu_delegate_->WillShowMenu(menu);
  }
}

void MenubarController::CloseMenu() {
  if (menu_runner_.get())
    menu_runner_->Cancel();
}

bool MenubarController::IsShowing() const {
  return menu_runner_.get() && menu_runner_->IsRunning();
}

bool MenubarController::ShouldExecuteCommandWithoutClosingMenu(int id,
                                                     const ui::Event& e) {
  if (IsBookmarkCommand(id) || IsVivaldiMenuItem(id)) {
    bookmark_menu_delegate_->ShouldExecuteCommandWithoutClosingMenu(id, e);
  }
  return IsItemPersistent(id);
}

// We want all menus to open under or over the menu bar to prevent long menus
// from opening left or right to the menu bar button. This would prevent proper
// bar navigation.
bool MenubarController::ShouldTryPositioningBesideAnchor() const {
  return false;
}

bool MenubarController::VivaldiShouldTryPositioningInMenuBar() const {
  return true;
}

void MenubarController::ExecuteCommand(int id, int mouse_event_flags) {
  if (IsBookmarkCommand(id) || IsVivaldiMenuItem(id)) {
    bookmark_menu_delegate_->ExecuteCommand(id, mouse_event_flags);
  } else {
    extensions::MenubarMenuAPI::SendAction(
        GetProfile(), id, mouse_event_flags, false);
  }
}

// This happens only when a menu is closed and no new is opened elsewhere.
void MenubarController::OnMenuClosed(views::MenuItemView* menu) {
  extensions::MenubarMenuAPI::SendClose(GetProfile());
}

bool MenubarController::IsTriggerableEvent(views::MenuItemView* menu,
                                 const ui::Event& e) {
  return IsBookmarkCommand(menu->GetCommand())
             ? bookmark_menu_delegate_->IsTriggerableEvent(menu, e)
             : MenuDelegate::IsTriggerableEvent(menu, e);
}

void MenubarController::VivaldiSelectionChanged(views::MenuItemView* menu) {
  if (IsBookmarkCommand(menu->GetCommand())) {
    bookmark_menu_delegate_->VivaldiSelectionChanged(menu);
  }
}

bool MenubarController::ShowContextMenu(views::MenuItemView* source,
                                        int command_id,
                                        const gfx::Point& p,
                                        ui::mojom::MenuSourceType source_type) {
  return IsBookmarkCommand(command_id)
             ? bookmark_menu_delegate_->ShowContextMenu(source, command_id, p,
                                                        source_type)
             : false;
}

bool MenubarController::IsItemChecked(int id) const {
  std::map<int, bool>::const_iterator it = id_to_checked_map_.find(id);
  return it != id_to_checked_map_.end() ? it->second : false;
}

bool MenubarController::IsCommandEnabled(int id) const {
  return IsItemEnabled(id);
}

bool MenubarController::GetAccelerator(int id,
                                       ui::Accelerator* accelerator) const {
  std::map<int, ui::Accelerator>::const_iterator it = id_to_accelerator_map_.find(id);
  if (it == id_to_accelerator_map_.end()) {
    return false;
  } else {
    *accelerator = it->second;
    return true;
  }
}

views::MenuItemView* MenubarController::GetVivaldiSiblingMenu(
    views::MenuItemView* menu,
    const gfx::Point& screen_point,
    gfx::Rect* rect,
    views::MenuAnchorPosition* anchor) {
  for (const MenubarMenuEntry& e : state_.siblings) {
    if (e.rect.Contains(screen_point)) {
      if (e.id == active_menu_id_) {
        return nullptr;
      }
      *rect = e.rect;
      *anchor = views::MenuAnchorPosition::kTopLeft;
      SetActiveMenu(e.id);
      return id_to_menu_map_[active_menu_id_];
    }
  }
  return nullptr;
}

views::MenuItemView* MenubarController::GetNextSiblingMenu(
    bool next,
    bool* has_mnemonics,
    gfx::Rect* rect,
    views::MenuAnchorPosition* anchor) {
  if (state_.siblings.size() == 0) {
    return nullptr;
  }

  unsigned int index = 0;
  for (const MenubarMenuEntry& e : state_.siblings) {
    if (e.id == active_menu_id_) {
      break;
    }
    index++;
  }
  if (next) {
    index++;
    if (index >= state_.siblings.size()) {
      index = 0;
    }
  } else {
    if (index == 0) {
      index = state_.siblings.size() - 1;
    } else {
      index--;
    }
  }
  *has_mnemonics = true;
  return GetVivaldiSiblingMenu(
      nullptr, state_.siblings.at(index).rect.origin(), rect, anchor);
}

void MenubarController::OnHover(const std::string& url) {
  extensions::MenubarMenuAPI::SendHover(GetProfile(),
      params_->properties.window_id, url);
}

void MenubarController::OnOpenBookmark(int64_t bookmark_id, int event_state) {
  extensions::MenubarMenuAPI::SendOpenBookmark(GetProfile(),
      params_->properties.window_id, bookmark_id, event_state);
}

void MenubarController::OnBookmarkAction(int64_t bookmark_id, int command) {
  extensions::MenubarMenuAPI::SendBookmarkAction(GetProfile(),
      params_->properties.window_id, bookmark_id, command);
}

// Note: This is not used by bookmarks. That uses a separate system.
void MenubarController::RequestFavicon(int id,
                                       int menu_id,
                                       const std::string& url) {
  if (!favicon_service_) {
    favicon_service_ = FaviconServiceFactory::GetForProfile(
        GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
    if (!favicon_service_)
      return;
  }

  favicon_base::FaviconImageCallback callback = base::BindOnce(
      &MenubarController::OnFaviconAvailable, base::Unretained(this), id,
          menu_id);

  favicon_service_->GetFaviconImageForPageURL(GURL(url), std::move(callback),
                                              &cancelable_task_tracker_);
}

void MenubarController::OnFaviconAvailable(
    int id,
    int menu_id,
    const favicon_base::FaviconImageResult& image_result) {
  if (!image_result.image.IsEmpty()) {
    std::map<int, views::MenuItemView*>::iterator it =
        id_to_menu_map_.find(menu_id);
    if (it != id_to_menu_map_.end()) {
      it->second->GetMenuItemByID(id)->SetIcon(
          ui::ImageModel::FromImage(image_result.image));
    }
  }
}

bool MenubarController::IsBookmarkMenu(int menu_id) {
  return bookmark_menu_id_ == menu_id;
}

int MenubarController::GetSelectedMenuId() {
  return selected_menu_id_;
}

bool MenubarController::IsItemEnabled(int id) const {
  // Note, we record the disabled entries as we normally have few disabled.
  std::map<int, bool>::const_iterator it = id_to_disabled_map_.find(id);
  return it != id_to_disabled_map_.end() ? !it->second : true;
}

bool MenubarController::IsItemPersistent(int id) const {
  std::map<int, bool>::const_iterator it = id_to_persistent_map_.find(id);
  return it != id_to_persistent_map_.end() ? it->second : false;
}

bool MenubarController::GetUrl(int id, std::string* url) const {
  std::map<int, std::string>::const_iterator it = id_to_url_map_.find(id);
  if (it == id_to_url_map_.end()) {
    return false;
  } else {
    *url = it->second;
    return true;
  }
}

}  // namespace vivaldi