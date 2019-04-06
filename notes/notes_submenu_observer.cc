// Copyright (c) 2016 Vivaldi. All rights reserved.

#include "notes/notes_submenu_observer.h"

#include "base/strings/utf_string_conversions.h"
#include "browser/menus/vivaldi_menu_enums.h"
#include "chrome/browser/profiles/profile.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "notes/notes_factory.h"
#include "notes/notes_model.h"
#include "renderer/vivaldi_render_messages.h"
#include "ui/base/l10n/l10n_util.h"
#include "vivaldi/grit/vivaldi_native_strings.h"

using content::BrowserThread;
using vivaldi::NotesModelFactory;

NotesSubMenuObserver::NotesSubMenuObserver(
    RenderViewContextMenuProxy* proxy,
    ui::SimpleMenuModel::Delegate* delegate)
    : proxy_(proxy), submenu_model_(delegate), delegate_(delegate) {
  DCHECK(proxy_);
}

NotesSubMenuObserver::~NotesSubMenuObserver() {}

void NotesSubMenuObserver::InitMenu(const content::ContextMenuParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowserContext* browser_context = proxy_->GetBrowserContext();

  vivaldi::Notes_Model* model =
    NotesModelFactory::GetForBrowserContext(browser_context);

  vivaldi::Notes_Node* node = model->main_node();
  for (int i = 0; i < node->child_count(); i++) {
    this->AddMenuItems(node->GetChild(i), &submenu_model_);
  }

  proxy_->AddSubMenu(IDC_VIV_CONTENT_INSERT_NOTE,
                     l10n_util::GetStringUTF16(IDS_VIV_CONTENT_INSERT_NOTE),
                     &submenu_model_);
}

void NotesSubMenuObserver::AddMenuItems(vivaldi::Notes_Node* node,
    ui::SimpleMenuModel* menu_model) {
  if (node->is_trash()) {
    return;
  }

  if (node->id() < min_notes_id_) {
    min_notes_id_ = node->id();
  }

  if (node->id() > max_notes_id_) {
    max_notes_id_ = node->id();
  }

  if (node->is_separator()) {
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  } else {
    const int kMaxMenuStringLength = 40;
    base::string16 data = node->GetTitle();
    if (data.length() == 0) {
      data = node->GetContent();
    }
    // Remove newlines inside string
    base::string16 title = base::CollapseWhitespace(data, false);
    // Remove spaces at start and end
    base::TrimWhitespace(title, base::TRIM_ALL, &title);
    // Truncate string if it is too long
    if (title.length() > kMaxMenuStringLength) {
      title = title.substr(0, kMaxMenuStringLength - 3) +
          base::UTF8ToUTF16("...");
    }
    if (node->is_folder()) {
      ui::SimpleMenuModel* child_menu_model
          = new ui::SimpleMenuModel(delegate_);
      menu_model->AddSubMenu(node->id(), title, child_menu_model);
      for (int i = 0; i < node->child_count(); i++) {
        this->AddMenuItems(node->GetChild(i), child_menu_model);
      }
    } else {
      menu_model->AddItem(node->id(), title);
    }
  }
}

bool NotesSubMenuObserver::IsCommandIdSupported(int command_id) {
  if (command_id == IDC_VIV_CONTENT_INSERT_NOTE ||
      (command_id >= min_notes_id_ && command_id <= max_notes_id_)) {
    return true;
  }
  return false;
}

bool NotesSubMenuObserver::IsCommandIdChecked(int command_id) {
  return false;
}

bool NotesSubMenuObserver::IsCommandIdEnabled(int command_id) {
  if (command_id == IDC_VIV_CONTENT_INSERT_NOTE ||
      (command_id >= min_notes_id_ && command_id <= max_notes_id_)) {
    return true;
  }

  return false;
}

vivaldi::Notes_Node* GetNodeFromId(vivaldi::Notes_Node* node, int id) {
  if (node->id() == id) {
    return node;
  }
  int number_of_notes = node->child_count();
  for (int i = 0; i < number_of_notes; i++) {
    vivaldi::Notes_Node* childnode = GetNodeFromId(node->GetChild(i), id);
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
