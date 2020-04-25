// Copyright (c) 2016 Vivaldi. All rights reserved.

#include "notes/notes_submenu_observer.h"

#include "base/strings/utf_string_conversions.h"
#include "browser/menus/vivaldi_menu_enums.h"
#include "chrome/browser/profiles/profile.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "notes/notes_factory.h"
#include "notes/notes_model.h"
#include "renderer/vivaldi_render_messages.h"
#include "ui/base/l10n/l10n_util.h"
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
    root_id_(IDC_VIV_CONTENT_INSERT_NOTE) {
}

NotesSubMenuObserver::~NotesSubMenuObserver() {}

void NotesSubMenuObserver::InitMenu(const content::ContextMenuParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::BrowserContext* browser_context = proxy_->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);

  if (!profile->IsGuestSession()) {
    ui::SimpleMenuModel* menu_model = new ui::SimpleMenuModel(helper_.get());
    models_.push_back(base::WrapUnique(menu_model));
    vivaldi::Notes_Model* model =
        NotesModelFactory::GetForBrowserContext(browser_context);
    menumodel_to_note_map_[menu_model] = model->main_node();
    proxy_->AddSubMenu(root_id_,
      l10n_util::GetStringUTF16(IDS_VIV_CONTENT_INSERT_NOTE),
      menu_model);
    if (!helper_->SupportsDelayedLoading()) {
      PopulateModel(menu_model);
    }
  }
}

void NotesSubMenuObserver::PopulateModel(ui::SimpleMenuModel* menu_model) {
  vivaldi::Notes_Node* parent = menumodel_to_note_map_[menu_model];
#if defined(OS_MACOSX)
  bool underline_letter = false;
#else
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  bool underline_letter = profile->GetPrefs()->GetBoolean(
      vivaldiprefs::kBookmarksUnderlineMenuLetter);
#endif

  for (auto& node : parent->children()) {

    if (node->is_separator()) {
      menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    }
    else {
      base::string16 data = node->GetTitle();
      if (data.length() == 0) {
        data = node->GetContent();
      }
      // Remove newlines inside string
      base::string16 title = base::CollapseWhitespace(data, false);
      // Remove spaces at start and end
      base::TrimWhitespace(title, base::TRIM_ALL, &title);
      // Truncate string if it is too long
      if (title.length() > MAX_NOTES_MENUITEM_LENGTH) {
        title = title.substr(0, MAX_NOTES_MENUITEM_LENGTH - 3) +
            base::UTF8ToUTF16("...");
      }
      // Escape any '&' with a double set to prevent underlining.
      if (!underline_letter) {
        base::StringPiece16 s1 = base::StringPiece16(base::UTF8ToUTF16("&"));
        base::string16 s2 = base::UTF8ToUTF16("&&");
        base::ReplaceChars(title, s1, s2, &title);
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
  return command_id == root_id_;
}

bool NotesSubMenuObserver::IsCommandIdChecked(int command_id) {
  return false;
}

bool NotesSubMenuObserver::IsCommandIdEnabled(int command_id) {
  return command_id == root_id_;
}

vivaldi::Notes_Node* GetNodeFromId(vivaldi::Notes_Node* node, int id) {
  if (node->id() == id) {
    return node;
  }
  for (auto& it : node->children()) {
    vivaldi::Notes_Node* childnode = GetNodeFromId(it.get(), id);
    if (childnode) {
      return childnode;
    }
  }
  return nullptr;
}

void NotesSubMenuObserver::ExecuteCommand(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  vivaldi::Notes_Model* model =
      NotesModelFactory::GetForBrowserContext(proxy_->GetBrowserContext());
  vivaldi::Notes_Node* root = model->root_node();

  vivaldi::Notes_Node* node = GetNodeFromId(root, command_id);
  if (node) {
    content::RenderFrameHost* focused_frame =
        proxy_->GetWebContents()->GetFocusedFrame();

    if (focused_frame) {
      proxy_->GetWebContents()->GetRenderViewHost()->Send(
        new VivaldiMsg_InsertText(
            proxy_->GetWebContents()->GetRenderViewHost()->GetRoutingID(),
            node->GetContent()));
    }
  }
}
