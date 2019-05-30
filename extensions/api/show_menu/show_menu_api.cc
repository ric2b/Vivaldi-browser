//
// Copyright (c) 2014-2015 Vivaldi Technologies AS. All rights reserved.
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
#include "base/values.h"
#include "browser/menus/bookmark_sorter.h"
#include "browser/menus/bookmark_support.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/platform_apps/app_load_service.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
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
#include "extensions/common/manifest_constants.h"
#include "extensions/schema/show_menu.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/devtools/devtools_connector.h"
#include "ui/vivaldi_context_menu.h"
#include "ui/vivaldi_main_menu.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#define BOOKMARK_ID_BASE 100000

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

// static
void ShowMenuAPI::SendCommandExecuted(Profile* profile,
                                      int window_id,
                                      int command_id,
                                      const std::string& parameter) {
  ::vivaldi::BroadcastEvent(
      show_menu::OnMainMenuCommand::kEventName,
      show_menu::OnMainMenuCommand::Create(window_id, command_id, parameter),
      profile);
}

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
                                 int old_index,
                                 const bookmarks::BookmarkNode* new_parent,
                                 int new_index) override {}
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                                 const bookmarks::BookmarkNode* parent,
                                 int index) override {}
  void BookmarkNodeRemoved(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* parent,
      int old_index,
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
  void HandleDeveloperToolsCommand(int command_id);
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

void VivaldiMenuController::Show() {
  // Populate menu
  for (std::vector<show_menu::MenuItem>::const_iterator it =
           params_->items.begin();
       it != params_->items.end(); ++it) {
    const show_menu::MenuItem& menuitem = *it;
    PopulateModel(&menuitem, &menu_model_, -1);
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
        extensions::DevtoolsConnectorAPI* api =
          extensions::DevtoolsConnectorAPI::GetFactoryInstance()->Get(
            Profile::FromBrowserContext(profile_));
        DCHECK(api);
        api->CloseAllDevtools();

        extensions::VivaldiUtilitiesAPI* utils_api =
            extensions::VivaldiUtilitiesAPI::GetFactoryInstance()->Get(
                profile_);
        DCHECK(utils_api);
        utils_api->CloseAllThumbnailWindows();

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
      if (root && root->child_count() > 0) {
        const bookmarks::BookmarkNode* vivaldi_root = root->GetChild(0);
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
    nodes.reserve(node->child_count());
    for (int i = 0; i < node->child_count(); ++i) {
      nodes.push_back(const_cast<bookmarks::BookmarkNode*>(node->GetChild(i)));
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
  if (command_id >= BOOKMARK_ID_BASE) {
    return true;
  }
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
  if (IsDeveloperTools(command_id)) {
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

ExtensionFunction::ResponseAction ShowMenuSetupMainMenuFunction::Run() {
  auto params = vivaldi::show_menu::SetupMainMenu::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());
#if defined(OS_MACOSX)
  if (params->mode == "menubar" || params->mode == "tabs" ||
      params->mode == "update" || params->mode == "bookmarks") {
    // Mac needs to update the menu even with no open windows. So we allow
    // a nullptr profile when calling the api.
    Profile* profile = Profile::FromBrowserContext(browser_context());
    ::vivaldi::CreateVivaldiMainMenu(profile, &params->items, params->mode);
    return RespondNow(NoArguments());
  }
  return RespondNow(Error("Invalid mode - " + params->mode));
#else
  return RespondNow(Error("NOT IMPLEMENTED"));
#endif  // defined(OS_MACOSX)
}

}  // namespace extensions
