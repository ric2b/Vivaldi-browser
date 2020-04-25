//
// Copyright (c) 2014-2019 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "extensions/api/show_menu/show_menu_api.h"

#include <map>
#include <math.h>
#include <memory>
#include <string>
#include <utility>

#include "app/vivaldi_resources.h"
#include "base/base64.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "browser/menus/bookmark_sorter.h"
#include "browser/menus/bookmark_support.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/platform_apps/app_load_service.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/favicon/core/favicon_service.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "content/public/browser/web_contents.h"
#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/schema/show_menu.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/devtools/devtools_connector.h"
#include "ui/gfx/text_elider.h"
#include "ui/vivaldi_context_menu.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#define BOOKMARK_ID_BASE 100000

constexpr size_t kMaxAppNameLength = 30;

namespace gfx {
class Rect;
}

namespace extensions {

using content::BrowserContext;

namespace show_menu = vivaldi::show_menu;
//namespace values = manifest_values;

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

}  // namespace

// static
void ShowMenuAPI::SendOpen(Profile* profile) {
  ::vivaldi::BroadcastEvent(show_menu::OnOpen::kEventName,
                            show_menu::OnOpen::Create(), profile);
}

// static
void ShowMenuAPI::SendClose(Profile* profile) {
  ::vivaldi::BroadcastEvent(show_menu::OnClose::kEventName,
                            show_menu::OnClose::Create(), profile);
}

// static
void ShowMenuAPI::SendUrlHighlighted(Profile* profile, const std::string& url) {
  ::vivaldi::BroadcastEvent(show_menu::OnUrlHighlighted::kEventName,
                            show_menu::OnUrlHighlighted::Create(url), profile);
}

// static
void ShowMenuAPI::SendBookmarkActivated(Profile* profile,
                                        int id,
                                        int event_flags) {
  show_menu::Response response;
  response.id = id;
  response.ctrl = event_flags & ui::EF_CONTROL_DOWN ? true : false;
  response.shift = event_flags & ui::EF_SHIFT_DOWN ? true : false;
  response.alt = event_flags & ui::EF_ALT_DOWN ? true : false;
  response.command = event_flags & ui::EF_COMMAND_DOWN ? true : false;
  response.left = event_flags & ui::EF_LEFT_MOUSE_BUTTON ? true : false;
  response.right = event_flags & ui::EF_RIGHT_MOUSE_BUTTON ? true : false;
  response.center = event_flags & ui::EF_MIDDLE_MOUSE_BUTTON ? true : false;

  ::vivaldi::BroadcastEvent(show_menu::OnBookmarkActivated::kEventName,
                            show_menu::OnBookmarkActivated::Create(response),
                            profile);
}

// static
void ShowMenuAPI::SendAddBookmark(Profile* profile, int id) {
  ::vivaldi::BroadcastEvent(show_menu::OnAddBookmark::kEventName,
                            show_menu::OnAddBookmark::Create(id), profile);
}

namespace {

namespace ShowContextMenu = vivaldi::show_menu::ShowContextMenu;

class VivaldiMenuController : public ui::SimpleMenuModel::Delegate,
                              public bookmarks::BookmarkModelObserver {
 public:
  struct BookmarkFolder {
    int node_id;
    int menu_id;
    bool complete;
  };

  VivaldiMenuController(content::WebContents* web_contents,
                        scoped_refptr<ShowMenuShowContextMenuFunction> fun,
                        std::unique_ptr<ShowContextMenu::Params> params);
  ~VivaldiMenuController() override;

  void Show();

  // ui::SimpleMenuModel::Delegate
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsItemForCommandIdDynamic(int command_id) const override;
  base::string16 GetLabelForCommandId(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  bool GetIconForCommandId(int command_id, gfx::Image* icon) const override;
  void VivaldiCommandIdHighlighted(int command_id) override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void OnMenuWillShow(ui::SimpleMenuModel* source) override;
  void MenuClosed(ui::SimpleMenuModel* source) override;

  // bookmarks::BookmarkModelObserver
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override {}
  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                                 const bookmarks::BookmarkNode* old_parent,
                                 size_t old_index,
                                 const bookmarks::BookmarkNode* new_parent,
                                 size_t new_index) override {}
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                                 const bookmarks::BookmarkNode* parent,
                                 size_t index) override {}
  void BookmarkNodeRemoved(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* parent,
      size_t old_index,
      const bookmarks::BookmarkNode* node,
      const std::set<GURL>& no_longer_bookmarked) override {}
  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkNodeFaviconChanged(bookmarks::BookmarkModel* model,
                                  const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkAllUserNodesRemoved(bookmarks::BookmarkModel* model,
                                const std::set<GURL>& removed_urls) override {}

 private:
  const vivaldi::show_menu::MenuItem* getItemByCommandId(int command_id) const;
  void PopulateModel(const vivaldi::show_menu::MenuItem* menuitem,
                     ui::SimpleMenuModel* menu_model,
                     int parent_id);
  void SanitizeModel(ui::SimpleMenuModel* menu_model);
  void AddAddTabToBookmarksMenuItem(ui::SimpleMenuModel* menu_model, int id);
  void SetupBookmarkFolder(const bookmarks::BookmarkNode* node,
                           bookmarks::BookmarkModel* model,
                           int parent_id,
                           bool is_top_folder,
                           int offset_in_folder,
                           ui::SimpleMenuModel* menu_model);
  void PopulateBookmarkFolder(ui::SimpleMenuModel* menu_model,
                              int node_id,
                              int offset);
  bookmarks::BookmarkModel* GetBookmarkModel();
  void EnableBookmarkObserver(bool enable);
  bool IsBookmarkSeparator(const bookmarks::BookmarkNode* node);
  bool HasDeveloperTools();
  bool IsDeveloperTools(int command_id) const;
  bool IsPWAItem(int command_id) const;
  void HandleDeveloperToolsCommand(int command_id);
  void HandlePWACommand(int command_id);
  const Extension* GetExtension() const;
  void LoadFavicon(int command_id, const std::string& url);
  void OnFaviconDataAvailable(
      int command_id,
      const favicon_base::FaviconImageResult& image_result);
  void SendMenuResult(int command_id, int event_flags);

  content::WebContents* web_contents_;  // Not owned by us.
  Profile* profile_;  // Not owned by us.
  scoped_refptr<ShowMenuShowContextMenuFunction> fun_;
  std::unique_ptr<ShowContextMenu::Params> params_;
  int menu_x;
  int menu_y;

  // Loading favicons
  base::CancelableTaskTracker cancelable_task_tracker_;
  favicon::FaviconService* favicon_service_;

  ui::SimpleMenuModel menu_model_;
  std::unique_ptr<::vivaldi::VivaldiContextMenu> menu_;
  std::vector<std::unique_ptr<ui::SimpleMenuModel>> models_;
  base::string16 separator_;
  base::string16 separator_description_;
  std::unique_ptr<::vivaldi::BookmarkSorter> bookmark_sorter_;
  std::map<int, std::string*> url_map_;
  std::map<ui::SimpleMenuModel*, BookmarkFolder> bookmark_menu_model_map_;
  // State variables to reduce lookups in url_map_
  int current_highlighted_id_;
  bool is_url_highlighted_;
  // Initial selection
  int initial_selected_id_;
  bool is_shown_;
  ::vivaldi::BookmarkSupport bookmark_support_;
};

VivaldiMenuController::VivaldiMenuController(
    content::WebContents* web_contents,
    scoped_refptr<ShowMenuShowContextMenuFunction> fun,
    std::unique_ptr<ShowContextMenu::Params> params)
    : web_contents_(web_contents),
      profile_(Profile::FromBrowserContext(web_contents_->GetBrowserContext())),
      fun_(std::move(fun)),
      params_(std::move(params)),
      favicon_service_(nullptr),
      menu_model_(this),
      current_highlighted_id_(-1),
      is_url_highlighted_(false),
      initial_selected_id_(-1),
      is_shown_(false) {
  blink::WebFloatPoint p(params_->properties.left, params_->properties.top);
  p = ::vivaldi::FromUICoordinates(web_contents, p);
  menu_x = round(p.x);
  menu_y = round(p.y);
}

VivaldiMenuController::~VivaldiMenuController() {
  EnableBookmarkObserver(false);
}

// Returns the appropriate menu label for the IDC_INSTALL_PWA command if
// available.
// Copied from app_menu_model.cc.
base::Optional<base::string16> GetInstallPWAAppMenuItemName(Browser* browser) {
  WebContents* web_contents =
    browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return base::nullopt;
  base::string16 app_name =
    banners::AppBannerManager::GetInstallableWebAppName(web_contents);
  if (app_name.empty())
    return base::nullopt;
  return l10n_util::GetStringFUTF16(IDS_INSTALL_TO_OS_LAUNCH_SURFACE, app_name);
}

void VivaldiMenuController::Show() {
  // Populate menu
  for (std::vector<show_menu::MenuItem>::const_iterator it =
           params_->items.begin();
       it != params_->items.end(); ++it) {
    const show_menu::MenuItem& menuitem = *it;
    PopulateModel(&menuitem, &menu_model_, -1);
  }
  Browser* browser =
      ::vivaldi::FindBrowserForEmbedderWebContents(web_contents_);
  if (browser &&
      VivaldiRuntimeFeatures::IsEnabled(browser->profile(), "install_pwa")) {
    const extensions::Extension* pwa =
      extensions::util::GetPwaForSecureActiveTab(browser);
    if (pwa) {
      menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
      menu_model_.AddItem(
          IDC_OPEN_IN_PWA_WINDOW,
          l10n_util::GetStringFUTF16(
              IDS_OPEN_IN_APP_WINDOW,
              gfx::TruncateString(base::UTF8ToUTF16(pwa->name()),
                                  kMaxAppNameLength, gfx::CHARACTER_BREAK)));
    } else {
      base::Optional<base::string16> install_pwa_item_name =
        GetInstallPWAAppMenuItemName(browser);
      if (install_pwa_item_name) {
        menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
        menu_model_.AddItem(IDC_INSTALL_PWA, *install_pwa_item_name);

        menu_model_.AddItemWithStringId(IDC_CREATE_SHORTCUT,
                                        IDS_ADD_TO_OS_LAUNCH_SURFACE);
      }
    }
  }

  if (HasDeveloperTools()) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    // NOTE(pettern): Reload will not work with our app, disable it for now.
//    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP,
//                                    IDS_CONTENT_CONTEXT_RELOAD_PACKAGED_APP);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP,
                                    IDS_CONTENT_CONTEXT_RESTART_APP);
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_INSPECTELEMENT,
                                    IDS_CONTENT_CONTEXT_INSPECTELEMENT);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE,
                                    IDS_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE);
  }

  SanitizeModel(&menu_model_);

  content::ContextMenuParams menu_params;
  menu_params.x = menu_x;
  menu_params.y = menu_y;
  menu_.reset(::vivaldi::CreateVivaldiContextMenu(
      web_contents_, &menu_model_, menu_params));
  ShowMenuAPI::SendOpen(profile_);
  menu_->Show();
}

void VivaldiMenuController::OnMenuWillShow(ui::SimpleMenuModel* source) {
  if (!is_shown_) {
    is_shown_ = true;
    if (source == &menu_model_ && initial_selected_id_ != -1) {
      menu_->SetSelectedItem(initial_selected_id_);
    }
  }

  std::map<ui::SimpleMenuModel*, BookmarkFolder>::iterator it
      = bookmark_menu_model_map_.find(source);
  if (it != bookmark_menu_model_map_.end()) {
    BookmarkFolder folder = it->second;
    if (!folder.complete) {
      folder.complete = true;
      bookmark_menu_model_map_[source] = folder;
      PopulateBookmarkFolder(source, folder.node_id, -1);
      menu_->UpdateMenu(source, folder.menu_id);
      if (source->GetItemCount() > 0) {
        // Only auto select the first time in the sub menu if it is not a
        // bookmark item (otherwise we will break keyboard navigation) and it
        // is not a sub folder (in that case we could end up recursively opening
        // its first child etc).
        if (source->GetTypeAt(0) != ui::MenuModel::TYPE_SUBMENU &&
          source->GetCommandIdAt(0) < BOOKMARK_ID_BASE) {
          menu_->SetSelectedItem(source->GetCommandIdAt(0));
        }
      }
    }
  }
}

void VivaldiMenuController::MenuClosed(ui::SimpleMenuModel* source) {
  source->SetMenuModelDelegate(nullptr);
  if (source == &menu_model_) {
    SendMenuResult(-1, 0);
    ShowMenuAPI::SendClose(profile_);
    delete this;
  }
}

bool VivaldiMenuController::HasDeveloperTools() {
  const extensions::Extension* platform_app = GetExtension();
  return extensions::Manifest::IsUnpackedLocation(platform_app->location()) ||
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDebugPackedApps);
}

bool VivaldiMenuController::IsDeveloperTools(int command_id) const {
  return /* command_id == IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP ||*/
         command_id == IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP ||
         command_id == IDC_CONTENT_CONTEXT_INSPECTELEMENT ||
         command_id == IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE;
}

bool VivaldiMenuController::IsPWAItem(int command_id) const {
  return command_id == IDC_INSTALL_PWA || command_id == IDC_CREATE_SHORTCUT ||
         command_id == IDC_OPEN_IN_PWA_WINDOW;
}

void VivaldiMenuController::HandlePWACommand(int command_id) {
  Browser* browser =
      ::vivaldi::FindBrowserForEmbedderWebContents(web_contents_);

  switch (command_id) {
  case IDC_CREATE_SHORTCUT:
    chrome::CreateBookmarkAppFromCurrentWebContents(browser,
      true /* force_shortcut_app */);
    break;
  case IDC_INSTALL_PWA:
    chrome::CreateBookmarkAppFromCurrentWebContents(browser,
      false /* force_shortcut_app */);
    break;
  case IDC_OPEN_IN_PWA_WINDOW:
    ReparentSecureActiveTabIntoPwaWindow(browser);
    break;
  default:
    break;
  }
}

void VivaldiMenuController::HandleDeveloperToolsCommand(int command_id) {
  const extensions::Extension* platform_app = GetExtension();

  switch (command_id) {
    case IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP:
      if (platform_app && platform_app->is_platform_app()) {
        extensions::ExtensionSystem::Get(profile_)
            ->extension_service()
            ->ReloadExtension(platform_app->id());
      }
      break;

    case IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP:
      if (platform_app && platform_app->is_platform_app()) {
        DevtoolsConnectorAPI::CloseAllDevtools(profile_);

        VivaldiUtilitiesAPI::CloseAllThumbnailWindows(profile_);

        chrome::AttemptRestart();
      }
      break;

    case IDC_CONTENT_CONTEXT_INSPECTELEMENT: {
      DevToolsWindow::InspectElement(web_contents_->GetMainFrame(),
                                     menu_x, menu_y);
    } break;

    case IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE:
      if (platform_app && platform_app->is_platform_app()) {
        extensions::devtools_util::InspectBackgroundPage(platform_app,
                                                         profile_);
      }
      break;
  }
}

const Extension* VivaldiMenuController::GetExtension() const {
  ProcessManager* process_manager = ProcessManager::Get(profile_);
  return process_manager->GetExtensionForWebContents(web_contents_);
}

void VivaldiMenuController::PopulateModel(const show_menu::MenuItem* item,
                                          ui::SimpleMenuModel* menu_model,
                                          int parent_id) {
  // Offset the command ids into the range of extension custom commands
  // plus add one to allow -1 as a command id.
  int id = item->id + IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST + 1;
  const base::string16 label = base::UTF8ToUTF16(item->name);

  if (item->name.find("---") == 0) {
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  } else if (item->container_type == show_menu::CONTAINER_TYPE_BOOKMARKS) {
    EnableBookmarkObserver(true);
    bookmarks::BookmarkModel* model = GetBookmarkModel();

    // Get label for adding new bookmarks
    bookmark_support_.add_label = label;

    // Get default icons
    if (item->container_icons) {
      bookmark_support_.initIcons(*item->container_icons);
    }

    // Folder grouping for first menu level
    bool group_folders = item->container_group_folders ?
      *item->container_group_folders : true;

    ::vivaldi::BookmarkSorter::SortField sortField;
    switch (item->container_sort_field) {
      case show_menu::SORT_FIELD_NONE:
        sortField = ::vivaldi::BookmarkSorter::FIELD_NONE;
        break;
      case show_menu::SORT_FIELD_TITLE:
        sortField = ::vivaldi::BookmarkSorter::FIELD_TITLE;
        break;
      case show_menu::SORT_FIELD_URL:
        sortField = ::vivaldi::BookmarkSorter::FIELD_URL;
        break;
      case show_menu::SORT_FIELD_NICKNAME:
        sortField = ::vivaldi::BookmarkSorter::FIELD_NICKNAME;
        break;
      case show_menu::SORT_FIELD_DESCRIPTION:
        sortField = ::vivaldi::BookmarkSorter::FIELD_DESCRIPTION;
        break;
      case show_menu::SORT_FIELD_DATEADDED:
        sortField = ::vivaldi::BookmarkSorter::FIELD_DATEADDED;
        break;
    }

    ::vivaldi::BookmarkSorter::SortOrder sortOrder;
    switch (item->container_sort_order) {
      case show_menu::SORT_ORDER_NONE:
        sortOrder = ::vivaldi::BookmarkSorter::ORDER_NONE;
        break;
      case show_menu::SORT_ORDER_ASCENDING:
        sortOrder = ::vivaldi::BookmarkSorter::ORDER_ASCENDING;
        break;
      case show_menu::SORT_ORDER_DESCENDING:
        sortOrder = ::vivaldi::BookmarkSorter::ORDER_DESCENDING;
        break;
    }

    bookmark_sorter_.reset(new ::vivaldi::BookmarkSorter(sortField, sortOrder,
        group_folders));

    // Select root node
    int id = 0;
    if (item->container_id) {
      id = *item->container_id;
    } else {
      const bookmarks::BookmarkNode* root = model->root_node();
      if (root && !root->children().empty()) {
        const bookmarks::BookmarkNode* vivaldi_root = root->children()[0].get();
        id = vivaldi_root->id();
      }
    }

    if (id) {
      const bookmarks::BookmarkNode* node = bookmarks::GetBookmarkNodeByID(
          model, id);
      if (node) {
        int offset = item->container_offset ? *item->container_offset : -1;
        SetupBookmarkFolder(node, model, parent_id, true, offset, menu_model);
      }
    }

  } else {
    if (initial_selected_id_ == -1 && item->selected && *item->selected) {
      initial_selected_id_ = id;
    }
    int index = item->index ? *item->index : -1;
    bool load_image = false;
    bool visible = item->visible ? *item->visible : true;
    if (visible) {
      if (item->items) {
        ui::SimpleMenuModel* child_menu_model = new ui::SimpleMenuModel(this);
        models_.push_back(base::WrapUnique(child_menu_model));
        for (std::vector<show_menu::MenuItem>::const_iterator it =
                 item->items->begin();
             it != item->items->end(); ++it) {
          const show_menu::MenuItem& child = *it;
          PopulateModel(&child, child_menu_model, id);
        }
        SanitizeModel(child_menu_model);
        menu_model->AddSubMenu(id, label, child_menu_model);
        load_image = true;
      } else {
        if (item->type.get() && item->type->compare("checkbox") == 0) {
          menu_model->AddCheckItem(id, label);
        } else if (item->type.get() &&
                   item->type->compare("radiobutton") == 0) {
          menu_model->AddRadioItem(id, label, *item->radiogroup.get());
        } else {
          if (index >= 0 && index <= menu_model->GetItemCount()) {
            menu_model->InsertItemAt(index, id, label);
          } else {
            menu_model->AddItem(id, label);
          }
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

bookmarks::BookmarkModel* VivaldiMenuController::GetBookmarkModel() {
  return BookmarkModelFactory::GetForBrowserContext(profile_);
}

void VivaldiMenuController::EnableBookmarkObserver(bool enable) {
  if (bookmark_support_.observer_enabled != enable) {
    bookmark_support_.observer_enabled = enable;
    bookmarks::BookmarkModel* model = GetBookmarkModel();
    if (model) {
      if (enable)
        model->AddObserver(this);
      else
        model->RemoveObserver(this);
    }
  }
}

void VivaldiMenuController::AddAddTabToBookmarksMenuItem(
    ui::SimpleMenuModel* menu_model,
    int id) {
  if (!bookmark_support_.add_label.empty()) {
    menu_model->AddItem(id, bookmark_support_.add_label);
  }
}

void VivaldiMenuController::SetupBookmarkFolder(
    const bookmarks::BookmarkNode* node,
    bookmarks::BookmarkModel* model,
    int parent_id,
    bool is_top_folder,
    int offset_in_folder,
    ui::SimpleMenuModel* menu_model) {

  // We only support adding items from a folder
  if (node->is_folder()) {
#if defined(OS_MACOSX)
    // Mac does not yet support progressive loading. This code will only be used
    // in the bookmark bar making less of a problem.
    PopulateBookmarkFolder(menu_model, node->id(), offset_in_folder);
#else
    if (parent_id == -1) {
      // No parent. We must populate now
      PopulateBookmarkFolder(menu_model, node->id(), offset_in_folder);
    } else {
      // Delay population until parent menu item is about to open.
      BookmarkFolder folder;
      folder.node_id = node->id();
      folder.menu_id = parent_id;
      folder.complete = false;
      bookmark_menu_model_map_[menu_model] = folder;
    }
#endif
  }
}

void VivaldiMenuController::PopulateBookmarkFolder(
    ui::SimpleMenuModel* menu_model, int node_id, int offset) {
  bookmarks::BookmarkModel* model = GetBookmarkModel();
  const bookmarks::BookmarkNode* node = bookmarks::GetBookmarkNodeByID(
      model, node_id);
  // Strings for escaping of '&' to prevent it from underlining.
  const base::char16 s1[2] = {'&', 0};
  const base::string16 s2 = base::UTF8ToUTF16("&&");
#if defined(OS_MACOSX)
  bool underline_letter = false;
#else
  bool underline_letter = profile_->GetPrefs()->GetBoolean(
        vivaldiprefs::kBookmarksUnderlineMenuLetter);
#endif

  if (node && node->is_folder()) {

    // Call AddAddTabToBookmarksMenuItem() here for in front of bookmarks.
    std::vector<bookmarks::BookmarkNode*> nodes;
    nodes.reserve(node->children().size());
    for (auto& it: node->children()) {
      nodes.push_back(const_cast<bookmarks::BookmarkNode*>(it.get()));
    }
    bookmark_sorter_->sort(nodes);
    bookmark_sorter_->setGroupFolders(true); // Always grouping in sub menus.

    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    for (size_t i = offset < 0 ? 0 : offset; i < nodes.size(); ++i) {
      const bookmarks::BookmarkNode* child = nodes[i];
      if (IsBookmarkSeparator(child)) {
        if (bookmark_sorter_->isManualOrder()) {
          menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
        }
      } else if (child->is_url()) {
        int id = BOOKMARK_ID_BASE + child->id();

        base::string16 title(child->GetTitle());
        if (!underline_letter) {
          base::ReplaceChars(title, s1, s2, &title);
        }
        menu_model->AddItem(id, title);
        const gfx::Image& image = model->GetFavicon(child);
        menu_model->SetIcon(menu_model->GetIndexOfCommandId(id),
               image.IsEmpty() ? bookmark_support_.iconForNode(child) : image);
      }
      else if (child->is_folder()) {
        ui::SimpleMenuModel* child_menu_model;
        child_menu_model = new ui::SimpleMenuModel(this);

        int menu_id = BOOKMARK_ID_BASE + child->id();
#if defined(OS_MACOSX)
        PopulateBookmarkFolder(child_menu_model, child->id(), -1);
#else
        BookmarkFolder folder;
        folder.node_id = child->id();
        folder.menu_id = menu_id;
        folder.complete = false;
        bookmark_menu_model_map_[child_menu_model] = folder;
#endif
        models_.push_back(base::WrapUnique(child_menu_model));
        base::string16 title(child->GetTitle());
        if (!underline_letter) {
          base::ReplaceChars(title, s1, s2, &title);
        }
        const base::string16 label = title.length() == 0
          ? l10n_util::GetStringUTF16(IDS_VIV_NO_TITLE) : title;
        menu_model->AddSubMenu(menu_id, label, child_menu_model);
        menu_model->SetIcon(menu_model->GetIndexOfCommandId(menu_id),
            bookmark_support_.iconForNode(child));
      }
    }
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);

    if (offset < 0) {
      AddAddTabToBookmarksMenuItem(menu_model, BOOKMARK_ID_BASE + node_id);
    }

  }
}

bool VivaldiMenuController::IsBookmarkSeparator(
    const bookmarks::BookmarkNode* node) {
  if (separator_.length() == 0) {
    separator_ = base::UTF8ToUTF16("---");
    separator_description_ = base::UTF8ToUTF16("separator");
  }
  return node->GetTitle().compare(separator_) == 0 &&
      node->GetDescription().compare(separator_description_) == 0;
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

void VivaldiMenuController::BookmarkNodeFaviconChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  const gfx::Image& image = model->GetFavicon(node);
  if (!image.IsEmpty()) {
    menu_->SetIcon(image, BOOKMARK_ID_BASE + node->id());
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

  favicon_service_->GetFaviconImageForPageURL(GURL(url), std::move(callback),
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
  if (command_id >= BOOKMARK_ID_BASE) {
    return true;
  }
  const show_menu::MenuItem* item = getItemByCommandId(command_id);
  return item != nullptr || IsDeveloperTools(command_id) ||
         IsPWAItem(command_id);
}

bool VivaldiMenuController::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDC_INSTALL_PWA;
}

base::string16 VivaldiMenuController::GetLabelForCommandId(
    int command_id) const {
  switch (command_id) {
    case IDC_INSTALL_PWA: {
      Browser* browser =
        ::vivaldi::FindBrowserForEmbedderWebContents(web_contents_);
      if (browser) {
        return GetInstallPWAAppMenuItemName(browser).value();
      }
      return base::string16();
    }
    default: {
      const show_menu::MenuItem* item = getItemByCommandId(command_id);
      return base::UTF8ToUTF16(item ? item->name : std::string());
    }
  }
}

bool VivaldiMenuController::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  const show_menu::MenuItem* item = getItemByCommandId(command_id);
  if (item && item->shortcut) {
    *accelerator = ::vivaldi::ParseShortcut(*item->shortcut, true);
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

void VivaldiMenuController::VivaldiCommandIdHighlighted(int command_id) {
  if (current_highlighted_id_ != command_id) {
    current_highlighted_id_ = command_id;
    bool was_highlighted = is_url_highlighted_;
    is_url_highlighted_ = false;
    if (current_highlighted_id_ >= BOOKMARK_ID_BASE) {
      int id = command_id - BOOKMARK_ID_BASE;
      bookmarks::BookmarkModel* model = GetBookmarkModel();
      const bookmarks::BookmarkNode* node = bookmarks::GetBookmarkNodeByID(
          model, id);
      if (node) {
        ShowMenuAPI::SendUrlHighlighted(profile_, node->url().spec());
      }
      is_url_highlighted_ = true;
    } else {
      std::map<int, std::string*>::iterator it = url_map_.find(command_id);
      if (it != url_map_.end()) {
        ShowMenuAPI::SendUrlHighlighted(profile_, *it->second);
        is_url_highlighted_ = true;
      }
    }
    if (was_highlighted && !is_url_highlighted_) {
      ShowMenuAPI::SendUrlHighlighted(profile_, "");
    }
  }
}

void VivaldiMenuController::ExecuteCommand(int command_id, int event_flags) {
  if (IsPWAItem(command_id)) {
    HandlePWACommand(command_id);
  } else if (IsDeveloperTools(command_id)) {
    // These are the commands we only get when running with npm.
    // For JS, this menu has been canceled since we handle the actions here.
    HandleDeveloperToolsCommand(command_id);
    SendMenuResult(-1, 0);
  } else if (command_id >= BOOKMARK_ID_BASE) {
    int id = command_id - BOOKMARK_ID_BASE;

    bookmarks::BookmarkModel* model = GetBookmarkModel();
    const bookmarks::BookmarkNode* node = bookmarks::GetBookmarkNodeByID(
          model, id);
    if (node && node->is_folder()) {
      ShowMenuAPI::SendAddBookmark(profile_, id);
    } else {
      ShowMenuAPI::SendBookmarkActivated(profile_, id, event_flags);
    }
    SendMenuResult(-1, 0);
  } else {
    SendMenuResult(command_id, event_flags);
  }
  EnableBookmarkObserver(false);
}

const show_menu::MenuItem* VivaldiMenuController::getItemByCommandId(
    int command_id) const {
  return GetMenuItemById(&params_->items, TranslateCommandIdToMenuId(command_id));
}

void VivaldiMenuController::SendMenuResult(int command_id, int event_flags) {
  if (fun_) {
    fun_->SendResult(command_id, event_flags);
    fun_.reset();
  }
}

}  // namespace

ExtensionFunction::ResponseAction ShowMenuShowContextMenuFunction::Run() {
  auto params = vivaldi::show_menu::ShowContextMenu::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::WebContents* web_contents = GetSenderWebContents();
  DCHECK(web_contents);
  if (!web_contents)
    return RespondNow(Error("Missing WebContents"));

  if (web_contents->IsShowingContextMenu()) {
    return RespondNow(
        Error("Attempt to show a Vivaldi context menu while Chromium "
              "context menu is running. Check that oncontextmenu is set "
              "and call preventDefault() to block the standard menu"));
  }

  // Menu is deallocated when the main menu model is closed.
  VivaldiMenuController* menu =
      new VivaldiMenuController(web_contents, this, std::move(params));
  menu->Show();
  return RespondLater();
}

void ShowMenuShowContextMenuFunction::SendResult(int command_id,
                                                 int event_flags) {
  int id = (command_id < 0) ? -1 : TranslateCommandIdToMenuId(command_id);
  show_menu::Response response;
  response.id = id;
  response.ctrl = event_flags & ui::EF_CONTROL_DOWN ? true : false;
  response.shift = event_flags & ui::EF_SHIFT_DOWN ? true : false;
  response.alt = event_flags & ui::EF_ALT_DOWN ? true : false;
  response.command = event_flags & ui::EF_COMMAND_DOWN ? true : false;
  response.left = event_flags & ui::EF_LEFT_MOUSE_BUTTON ? true : false;
  response.right = event_flags & ui::EF_RIGHT_MOUSE_BUTTON ? true : false;
  response.center = event_flags & ui::EF_MIDDLE_MOUSE_BUTTON ? true : false;
  Respond(ArgumentList(vivaldi::show_menu::ShowContextMenu::Results::Create(response)));
}

}  // namespace extensions
