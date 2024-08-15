// Copyright (c) 2016 Vivaldi. All rights reserved.

#include "components/notes/notes_submenu_observer.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/l10n/l10n_util.h"

#include "browser/menus/vivaldi_menu_enums.h"
#include "components/notes/notes_factory.h"
#include "components/notes/notes_model.h"
#include "vivaldi/app/grit/vivaldi_native_strings.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#define MAX_NOTES_MENUITEM_LENGTH 40

using content::BrowserThread;
using vivaldi::NotesModelFactory;

NotesSubMenuObserver::NotesSubMenuObserver(
    RenderViewContextMenuProxy* proxy,
    RenderViewContextMenuBase::ToolkitDelegate* toolkit_delegate)
    : helper_(CreateSubMenuObserverHelper(this, toolkit_delegate)),
      proxy_(proxy),
      root_id_(IDC_VIV_CONTENT_INSERT_NOTE) {}

NotesSubMenuObserver::~NotesSubMenuObserver() {}

void NotesSubMenuObserver::SetRootModel(ui::SimpleMenuModel* model,
                                        int id,
                                        bool is_folder) {
  root_menu_model_ = model;
  root_id_ = id;
  root_is_folder_ = is_folder;
}

void NotesSubMenuObserver::InitMenu(const content::ContextMenuParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::BrowserContext* browser_context = proxy_->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);

  if (!profile->IsGuestSession()) {
    vivaldi::NotesModel* model =
        NotesModelFactory::GetForBrowserContext(browser_context);

    root_id_map_[root_id_] = true;

    // 'root_menu_model_' is set when we use configurable menus to allow
    // placement in all locations of the menu tree.
    if (root_menu_model_) {
      // Register top nodes. Since we have a custom root menu we have to map all
      // children of the main node as well.
      for (auto& node : model->main_node()->children()) {
        root_id_map_[node->id()] = true;
      }
      menumodel_to_note_map_[root_menu_model_] = model->main_node();
      if (!root_is_folder_ || !helper_->SupportsDelayedLoading()) {
        // We have to populate right away if there is not folder to listen for
        // or when the system does not provide such a listen method.
        PopulateModel(root_menu_model_);
      }
    } else {
      ui::SimpleMenuModel* menu_model = new ui::SimpleMenuModel(helper_.get());
      models_.push_back(base::WrapUnique(menu_model));
      menumodel_to_note_map_[menu_model] = model->main_node();
      if (root_menu_model_) {
        root_menu_model_->AddSubMenu(
            root_id_, l10n_util::GetStringUTF16(IDS_VIV_CONTENT_INSERT_NOTE),
            menu_model);
      } else {
        proxy_->AddSubMenu(
            root_id_, l10n_util::GetStringUTF16(IDS_VIV_CONTENT_INSERT_NOTE),
            menu_model);
      }
      if (!helper_->SupportsDelayedLoading()) {
        PopulateModel(menu_model);
      }
    }
  }
}

int NotesSubMenuObserver::GetRootId() {
  if (root_menu_model_ && !root_is_folder_) {
    // Inline layout (no top root). Use the id of the first child.
    content::BrowserContext* browser_context = proxy_->GetBrowserContext();
    vivaldi::NotesModel* model =
        NotesModelFactory::GetForBrowserContext(browser_context);
    if (model->main_node() && model->main_node()->children().front()) {
      return model->main_node()->children().front()->id();
    }
  }
  return root_id_;
}

void NotesSubMenuObserver::RootMenuWillOpen() {
  if (root_menu_model_ && root_is_folder_) {
    helper_->OnMenuWillShow(root_menu_model_);
  }
}

void NotesSubMenuObserver::PopulateModel(ui::SimpleMenuModel* menu_model) {
  const vivaldi::NoteNode* parent = menumodel_to_note_map_[menu_model];
#if BUILDFLAG(IS_MAC)
  bool underline_letter = false;
#else
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  bool underline_letter = profile->GetPrefs()->GetBoolean(
      vivaldiprefs::kBookmarksUnderlineMenuLetter);
#endif

  for (const auto& node : parent->children()) {
    if (node->is_separator()) {
      menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    } else {
      std::u16string data = node->GetTitle();
      if (data.length() == 0) {
        data = node->GetContent();
      }
      // Remove newlines inside string
      std::u16string title = base::CollapseWhitespace(data, false);
      // Remove spaces at start and end
      base::TrimWhitespace(title, base::TRIM_ALL, &title);
      // Truncate string if it is too long
      if (title.length() > MAX_NOTES_MENUITEM_LENGTH) {
        title = title.substr(0, MAX_NOTES_MENUITEM_LENGTH - 3) + u"...";
      }

      bool underline = underline_letter;
      if (underline) {
        // Prevent underlining a space
        char16_t c = ui::GetMnemonic(title);
        if (c == ' ') {
          underline = false;
        }
      }
      if (!underline) {
        // Escape any '&' with a double set to prevent underlining.
        title = ui::EscapeMenuLabelAmpersands(title);
      }
      if (node->is_folder()) {
        ui::SimpleMenuModel* child_menu_model =
            new ui::SimpleMenuModel(helper_.get());
        models_.push_back(base::WrapUnique(child_menu_model));
        menumodel_to_note_map_[child_menu_model] = node.get();
        menu_model->AddSubMenu(node->id(), title, child_menu_model);
        if (!helper_->SupportsDelayedLoading()) {
          PopulateModel(child_menu_model);
        }
      } else {
        if (node->id() < min_notes_id_) {
          min_notes_id_ = node->id();
        }
        if (node->id() > max_notes_id_) {
          max_notes_id_ = node->id();
        }
        menu_model->AddItem(node->id(), title);
      }
    }
  }
}

bool NotesSubMenuObserver::IsCommandIdSupported(int command_id) {
  auto it = root_id_map_.find(command_id);
  return it != root_id_map_.end();
}

bool NotesSubMenuObserver::IsCommandIdChecked(int command_id) {
  return false;
}

bool NotesSubMenuObserver::IsCommandIdEnabled(int command_id) {
  return true;
}

void NotesSubMenuObserver::ExecuteCommand(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  vivaldi::NotesModel* model =
      NotesModelFactory::GetForBrowserContext(proxy_->GetBrowserContext());

  const vivaldi::NoteNode* node = model->GetNoteNodeByID(command_id);
  if (node) {
    auto* focused_frame = static_cast<content::RenderFrameHostImpl*>(
        proxy_->GetWebContents()->GetFocusedFrame());
    if (focused_frame) {
      focused_frame->GetVivaldiFrameService()->InsertText(
          base::UTF16ToUTF8(node->GetContent()));
    }
  }
}
