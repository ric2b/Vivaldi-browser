// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/sessions/vivaldi_sessions_api.h"
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/vivaldi_constants.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "browser/sessions/vivaldi_session_service.h"
#include "browser/sessions/vivaldi_session_utils.h"
#include "browser/vivaldi_browser_finder.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/session_service_commands.h"
#include "components/sessions/vivaldi_session_service_commands.h"
#include "components/datasource/vivaldi_image_store.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/api/tabs/tabs_private_api.h"
#include "extensions/schema/vivaldi_sessions.h"
#include "extensions/tools/vivaldi_tools.h"
#include "sessions/index_model.h"
#include "sessions/index_node.h"
#include "sessions/index_service_factory.h"
#include "sessions/index_storage.h"
#include "ui/vivaldi_browser_window.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

using content::BrowserContext;
using extensions::vivaldi::sessions_private::SessionAddOptions;
using extensions::vivaldi::sessions_private::SessionOpenOptions;
using sessions::Index_Model;
using sessions::Index_Node;
using sessions::IndexServiceFactory;

using extensions::vivaldi::sessions_private::ContentModel;
using extensions::vivaldi::sessions_private::WindowContent;
using extensions::vivaldi::sessions_private::WorkspaceContent;
using extensions::vivaldi::sessions_private::TabContent;

namespace {

typedef std::pair<Index_Node*, Index_Model*> NodeModel;

NodeModel GetNodeAndModel(BrowserContext* browser_context, int id) {
  Index_Model* model;
  Index_Node* node;

  model = IndexServiceFactory::GetForBrowserContext(browser_context);
  node = model->items_node() ?  model->items_node()->GetById(id) : nullptr;
  if (node) {
    return std::make_pair(node, model);
  } else {
    // Fallback for the case we do a backup to the internal kBackupNodeId.
    if (id == Index_Node::backup_node_id()) {
      node = model->root_node() ? model->root_node()->GetById(id) : nullptr;
      if (node) {
        return std::make_pair(node, model);
      }
    }
    return std::make_pair(nullptr, nullptr);
  }
}

Index_Node* FindNode(Index_Node* seed, const std::string& guid) {
  for(; seed; seed = seed->parent()) {
    if (seed->id() == Index_Node::items_node_id()) {
      return seed->GetByGuid(guid);
    }
  }
  return nullptr;
}

int MakeBackup(BrowserContext* browser_context) {
  sessions::WriteSessionOptions ctl;
  ctl.filename = "backup";

  NodeModel pair = GetNodeAndModel(
      browser_context, Index_Node::backup_node_id());
  if (pair.first) {
    // Node exists. Just update session file.
    // TODO: Perhaps rename old, write new, remove (or restore) old.
    int error_code = sessions::DeleteSessionFile(browser_context, pair.first);
    // Allow a missing session file when we are deleting.
    if (error_code == sessions::kErrorFileMissing) {
      error_code = sessions::kNoError;
    }
    if (error_code == sessions::kNoError) {
      error_code = sessions::WriteSessionFile(browser_context, ctl);

      // Placeholder for transferring updated data to existing node.
      std::unique_ptr<Index_Node> tmp = std::make_unique<Index_Node>("", -1);
      SetNodeState(browser_context, ctl.path, true, tmp.get());
      tmp->SetFilename(ctl.filename);

      pair.second->Change(pair.first, tmp.get());
    }
    return error_code;
  } else {
    // Write node for the first time.
    int error_code = sessions::WriteSessionFile(browser_context, ctl);
    if (error_code == sessions::kNoError) {
      Index_Model* model = IndexServiceFactory::GetForBrowserContext(
          browser_context);
      std::unique_ptr<Index_Node> node = std::make_unique<Index_Node>(
          Index_Node::backup_node_guid(), Index_Node::backup_node_id());
      SetNodeState(browser_context, ctl.path, true, node.get());
      node->SetFilename(ctl.filename);
      model->Add(std::move(node), model->root_node(), 0, "");
    }
    return error_code;
  }
}

}  // namespace

namespace extensions {
using extensions::vivaldi::sessions_private::SessionItem;
using extensions::vivaldi::sessions_private::WorkspaceItem;
using extensions::vivaldi::sessions_private::GroupName;
using extensions::vivaldi::sessions_private::ContentModel;

SessionItem MakeAPITreeNode(Index_Node* node, Index_Node* parent) {
  SessionItem api_node;
  int id = node->id();

  api_node.id = id;
  if (node->type() == Index_Node::kFolder) {
    api_node.type = vivaldi::sessions_private::ItemType::kFolder;
  } else {
    api_node.type = parent->is_container()
                        ? vivaldi::sessions_private::ItemType::kHistory
                        : vivaldi::sessions_private::ItemType::kNode;
  }
  api_node.parent_id = parent->id();
  api_node.container_id = -1;
  if (parent->is_container()) {
    api_node.container_id = parent->id();
  } else if (parent->is_trash_folder()) {
    Index_Node* container_node = FindNode(parent, node->container_guid());
    if (container_node) {
      // So that a session can be restored into the correct container.
      api_node.container_id = container_node->id();
    }
  }
  api_node.name = base::UTF16ToUTF8(node->GetTitle());
  api_node.create_date_js = node->create_time();
  api_node.modify_date_js = node->modify_time();
  api_node.windows = node->windows_count();
  api_node.tabs = node->tabs_count();
  api_node.quarantined = node->quarantine_count();
  std::vector<WorkspaceItem> workspaces;
  for (const auto& elm : node->workspaces()) {
    const base::Value::Dict* dict = elm.GetIfDict();
    if (dict) {
      std::optional<bool> active = dict->FindBool("active");
      // Test for has_value() as the flag was not present in the first version.
      if (!active.has_value() || active.value() == true) {
        std::optional<double> workspace_id = dict->FindDouble("id");
        if (workspace_id.has_value()) {
          const std::string* name = dict->FindString("name");
          const std::string* icon = dict->FindString("icon");
          const std::string* emoji = dict->FindString("emoji");
          WorkspaceItem workspace;
          workspace.id = workspace_id.value();
          if (name) {
            workspace.name = *name;
          }
          if (icon) {
            workspace.icon = *icon;
          }
          if (emoji) {
            workspace.emoji = *emoji;
          }
          workspaces.push_back(std::move(workspace));
        }
      }
    }
  }
  api_node.workspaces = std::move(workspaces);

  std::vector<GroupName> group_names;
  for (const auto elm : node->group_names()) {
    GroupName entry;
    entry.id = elm.first;
    entry.name = elm.second.GetString();
    group_names.push_back(std::move(entry));
  }
  api_node.group_names = std::move(group_names);

  if (node->is_folder() || node->is_container()) {
    std::vector<SessionItem> children;
    for (auto& child : node->children()) {
      children.push_back(MakeAPITreeNode(child.get(), node));
    }
    api_node.children = std::move(children);
  }

  return api_node;
}

void SortTabs(std::vector<TabContent>& tabs) {
  std::sort(tabs.begin(), tabs.end(),
            [](TabContent& tab1,
               TabContent& tab2) {
              return tab1.index < tab2.index;
            });
}

void MakeAPIContentModel(content::BrowserContext* browser_context,
                         Index_Node* node,
                         ContentModel& model) {
  model.id = node->id();

  base::FilePath path = sessions::GetPathFromNode(browser_context, node);
  sessions::SessionContent content;
  sessions::GetContent(path, content);

  for (const auto& elm : node->workspaces()) {
    const base::Value::Dict* dict = elm.GetIfDict();
    if (dict) {
      std::optional<double> workspace_id = dict->FindDouble("id");
      if (workspace_id.has_value()) {
        WorkspaceContent workspace;
        workspace.id = workspace_id.value();
        const std::string* name = dict->FindString("name");
        const std::string* icon = dict->FindString("icon");
        const std::string* emoji = dict->FindString("emoji");
        if (name) {
          workspace.name = *name;
        }
        if (icon) {
          workspace.icon = *icon;
        }
        if (emoji) {
          workspace.emoji = *emoji;
        }
        model.workspaces.push_back(std::move(workspace));
      }
    }
  }

  for (auto wit = content.windows.begin();
       wit != content.windows.end();
       ++wit) {
    // Tab stack titles are no longer saved to window ext data with VB-23686.
    // We read them should the file be an older version and apply content to
    // the tab elements below.
    std::unique_ptr<base::Value::Dict> tab_stacks(
        sessions::GetTabStackTitles(wit->second.get()));

    WindowContent window;
    for (auto tit = content.tabs.begin(); tit != content.tabs.end(); ++tit) {
      if (tit->second->window_id == wit->second->window_id) {
        // It can happen the index is out of bounds.
        // TODO: Examine why chrome allows that.
        int index = tit->second->current_navigation_index;
        if (index < 0) {
          index = 0;
        }
        unsigned long size = tit->second->navigations.size();
        if (size == 0) {
          LOG(ERROR) << "Content model. No navigation entries for tab";
          continue;
        }
        if (static_cast<unsigned long>(index) >= size) {
          index = size - 1;
        }
        const sessions::SerializedNavigationEntry& entry =
            tit->second->navigations.at(index);
        TabContent tab;
        tab.id = tit->second->tab_id.id();
        tab.index = tit->second->tab_visual_index;
        tab.url = entry.virtual_url().spec();
        tab.name = base::UTF16ToUTF8(entry.title());
        tab.pinned = tit->second->pinned;
        tab.quarantine = sessions::IsTabQuarantined(tit->second.get());
        tab.group = sessions::GetTabStackId(tit->second.get());
        // Stack name. Files before VB-23686 saved relevant entries in window
        // ext data while files after hold data in tab ext data.
        if (tab_stacks && !tab_stacks->empty()) {
          std::string* stack_name = tab_stacks->FindString(tab.group);
          if (stack_name) {
            tab.fixed_group_name = *stack_name;
          }
        }
        // Read from title from tab ext data.
        sessions::GetFixedTabTitles(tit->second.get(), tab.fixed_name,
            tab.fixed_group_name);

        std::optional<double> id = GetTabWorkspaceId(
            tit->second->viv_ext_data);
        if (id.has_value()) {
          // Add tab to workspace.
          bool match = false;
          for (auto wsit = model.workspaces.begin();
               !match && wsit != model.workspaces.end();
               ++wsit) {
            match = id.value() == wsit->id;
            if (match) {
              wsit->tabs.push_back(std::move(tab));
            }
          }
          if (!match) {
            // No workspace. We save the workspace id in the session tab even if
            // we choose not to save overall workspace information in the
            // session ("Include All Workspaces" is unchecked in save dialog).
            // When we load such a tab in the UI it will be opened in the
            // default workspace. We do the same here.
            window.tabs.push_back(std::move(tab));
          }
        } else {
          window.tabs.push_back(std::move(tab));
        }
      }
    }
    SortTabs(window.tabs);
    window.id = wit->second->window_id.id();
    window.quarantine = false;
    model.windows.push_back(std::move(window));
  }

  for (auto& workspace : model.workspaces) {
    SortTabs(workspace.tabs);
  }

}

static base::LazyInstance<BrowserContextKeyedAPIFactory<SessionsPrivateAPI>>::
    DestructorAtExit g_session_private = LAZY_INSTANCE_INITIALIZER;

SessionsPrivateAPI::SessionsPrivateAPI(content::BrowserContext* context)
  : browser_context_(context) {
  model_ = sessions::IndexServiceFactory::GetForBrowserContext(context);
  model_->AddObserver(this);
}

SessionsPrivateAPI::~SessionsPrivateAPI() {
  if (model_) {
    model_->RemoveObserver(this);
  }
}

// static
BrowserContextKeyedAPIFactory<SessionsPrivateAPI>*
SessionsPrivateAPI::GetFactoryInstance() {
  return g_session_private.Pointer();
}

void SessionsPrivateAPI::IndexModelBeingDeleted(Index_Model* model) {
  if (model == model_) {
    model_->RemoveObserver(this);
    model_ = nullptr;
  }
}

void SessionsPrivateAPI::IndexModelNodeAdded(Index_Model* model,
                                             Index_Node* node,
                                             int64_t parent_id, size_t index,
                                             const std::string& owner) {
  SessionsPrivateAPI::SendAdded(browser_context_, node, parent_id, index, owner);
}

void SessionsPrivateAPI::IndexModelNodeMoved(Index_Model* model, int64_t id,
                                             int64_t parent_id, size_t index) {
  SendMoved(browser_context_, id, parent_id, index);
}

void SessionsPrivateAPI::IndexModelNodeChanged(Index_Model* model,
                                               Index_Node* node) {
  SendChanged(browser_context_, node);
}

void SessionsPrivateAPI::IndexModelNodeRemoved(Index_Model* model, int64_t id) {
  SendDeleted(browser_context_, id);
}

//static
void SessionsPrivateAPI::SendAdded(
    content::BrowserContext* browser_context,
    Index_Node* node,
    int parent_id,
    int index,
    const std::string& owner) {
  vivaldi::sessions_private::SessionChangeData data;
  data.owner = owner;
  data.parent_id = parent_id;
  data.index = index;
  if (node->parent()->id()) {
    data.item = MakeAPITreeNode(node, node->parent());
  }
  ::vivaldi::BroadcastEvent(vivaldi::sessions_private::OnChanged::kEventName,
                            vivaldi::sessions_private::OnChanged::Create(
                              node->id(), vivaldi::sessions_private::SessionChange::kAdded,
                              data),
                            browser_context);
}

// static
void SessionsPrivateAPI::SendDeleted(
  content::BrowserContext* browser_context, int id) {
  vivaldi::sessions_private::SessionChangeData data;
  data.owner = "";
  data.parent_id = -1;
  data.index = -1;
  ::vivaldi::BroadcastEvent(vivaldi::sessions_private::OnChanged::kEventName,
                            vivaldi::sessions_private::OnChanged::Create(
                              id, vivaldi::sessions_private::SessionChange::kDeleted,
                              data),
                            browser_context);
}

// static
void SessionsPrivateAPI::SendChanged(
  content::BrowserContext* browser_context,
  Index_Node* node
) {
  vivaldi::sessions_private::SessionChangeData data;
  data.owner = "";
  data.parent_id = -1;
  data.index = -1;
  if (node->parent()->id()) {
    data.item = MakeAPITreeNode(node, node->parent());
  }
  ::vivaldi::BroadcastEvent(vivaldi::sessions_private::OnChanged::kEventName,
                            vivaldi::sessions_private::OnChanged::Create(
                              node->id(), vivaldi::sessions_private::SessionChange::kChanged,
                              data),
                            browser_context);
}

// static
void SessionsPrivateAPI::SendMoved(
  content::BrowserContext* browser_context,
  int id,
  int parent_id,
  int index
) {
  vivaldi::sessions_private::SessionChangeData data;
  data.owner = "";
  data.parent_id = parent_id;
  data.index = index;

  NodeModel pair = GetNodeAndModel(browser_context, id);
  if (pair.first) {
    data.item = MakeAPITreeNode(pair.first, pair.first->parent());
  }

  ::vivaldi::BroadcastEvent(vivaldi::sessions_private::OnChanged::kEventName,
                            vivaldi::sessions_private::OnChanged::Create(
                              id, vivaldi::sessions_private::SessionChange::kMoved,
                              data),
                            browser_context);
}

// static
void SessionsPrivateAPI::SendContentChanged(
  content::BrowserContext* browser_context,
  int id,
  ContentModel& content) {
  vivaldi::sessions_private::SessionChangeData data;
  data.owner = "";
  data.parent_id = -1;
  data.index = -1;
  data.content = std::move(content);
  ::vivaldi::BroadcastEvent(vivaldi::sessions_private::OnChanged::kEventName,
                            vivaldi::sessions_private::OnChanged::Create(
                              id,
                              vivaldi::sessions_private::SessionChange::kContent,
                              data),
                            browser_context);
}

// static
void SessionsPrivateAPI::SendOnPersistentLoad(
    content::BrowserContext* browser_context,
    bool state) {
  ::vivaldi::BroadcastEvent(
      vivaldi::sessions_private::OnPersistentLoad::kEventName,
      vivaldi::sessions_private::OnPersistentLoad::Create(state),
      browser_context);
}

SessionsPrivateAddFunction::SessionsPrivateAddFunction() = default;
SessionsPrivateAddFunction::~SessionsPrivateAddFunction() = default;

ExtensionFunction::ResponseAction SessionsPrivateAddFunction::Run() {
  using vivaldi::sessions_private::SessionAddOptions;
  using vivaldi::sessions_private::Add::Params;
  namespace Results = vivaldi::sessions_private::Add::Results;

  params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const SessionAddOptions& options = params->options;

  // Special test for backup operation.
  if (options.backup != vivaldi::sessions_private::BackupType::kNone) {
    int error_code = MakeBackup(browser_context());
    if (error_code == sessions::kNoError &&
        options.backup == vivaldi::sessions_private::BackupType::kAutosave) {
      error_code = sessions::AutoSaveFromBackup(browser_context());
    }
    return RespondNow(ArgumentList(Results::Create(error_code)));
  }

  if (options.window_id.has_value())
    ctl_.window_id = options.window_id.value();
  if (options.ids.has_value())
    ctl_.ids = options.ids.value();
  if (options.from_id.has_value())
    ctl_.from_id = options.from_id.value();
  ctl_.filename = options.filename;

  // Collects all the thumbnails URLs used by the session to be saved,
  // read their content into the Batch and store them in the session file.
  VivaldiImageStore::BatchRead(browser_context(),
      sessions::CollectThumbnailUrls(browser_context(), ctl_),
      base::BindOnce([](scoped_refptr<SessionsPrivateAddFunction> fn,
        VivaldiImageStore::Batch batch) -> void
  {
    // called, when the batch is ready
    const SessionAddOptions& options = fn->params->options;
    NodeModel pair = GetNodeAndModel(fn->browser_context(), options.parent_id);
    if (!pair.first) {
      fn->Respond(fn->ArgumentList(Results::Create(sessions::kErrorUnknownId)));
      return;
    }

    fn->ctl_.thumbnails.swap(batch);

    Profile* profile = Profile::FromBrowserContext(fn->browser_context());
    bool with_workspaces = profile->GetPrefs()->GetBoolean(
          vivaldiprefs::kSessionsSaveAllWorkspaces);

    int error_code = sessions::WriteSessionFile(fn->browser_context(), fn->ctl_);
    if (error_code == sessions::kNoError) {
      int id = Index_Node::GetNewId();
      std::unique_ptr<Index_Node> node = std::make_unique<Index_Node>(
          base::Uuid::GenerateRandomV4().AsLowercaseString(), id);
      SetNodeState(fn->browser_context(), fn->ctl_.path, true, node.get());
      if (!with_workspaces) {
        base::Value::List workspaces;
        node->SetWorkspaces(std::move(workspaces));
      }
      node->SetTitle(base::UTF8ToUTF16(options.name));
      node->SetFilename(fn->ctl_.filename);
      // SetNodeState sets create time to now. Revert that when copying.
      if (options.from_id.has_value()) {
        Index_Node* from = pair.second->root_node()->GetById(
            options.from_id.value());
        if (from) {
          node->SetCreateTime(from->create_time());
        }
      }
      pair.second->Add(std::move(node), pair.first, options.index, options.owner);
    }

    fn->Respond(fn->ArgumentList(Results::Create(error_code)));
  }, scoped_refptr<SessionsPrivateAddFunction>(this)));

  return RespondLater();
}

ExtensionFunction::ResponseAction SessionsPrivateGetAllFunction::Run() {
  Index_Model* model =
      IndexServiceFactory::GetForBrowserContext(browser_context());
  if (model->loaded()) {
    Piggyback();
    SendResponse(model);
  } else {
    AddRef();  // Balanced in IndexModelLoaded().
    model->AddObserver(this);
    model->Load();
    return RespondLater();
  }
  return AlreadyResponded();
}

void SessionsPrivateGetAllFunction::IndexModelLoaded(Index_Model* model) {
  SendResponse(model);
  model->RemoveObserver(this);
  Piggyback();
  Release();  // Balanced in Run().
}

// As name suggests this is not the best place to handle this, but it makes the
// code simple. If there is a saved persistent session (a session with only
// pinned and ws tabs) on startup it will be applied to the first regular
// browser. A browser will also try this itself but it may happen before the
// session model is loaded. This kind of session is only set up for Mac at the
// moment but it may be expanded due to extensions that allow Vivaldi to run in
// background with not windows for all Linux and Windows as well.
void SessionsPrivateGetAllFunction::Piggyback() {
  for (Browser* browser : *BrowserList::GetInstance()) {
    VivaldiBrowserWindow* window = VivaldiBrowserWindow::FromBrowser(browser);
    if (window && window->type() == VivaldiBrowserWindow::NORMAL) {
      // Open in first browser with correct profile.
      int error_code = sessions::OpenPersistentTabs(browser, true);
      if (error_code != sessions::kErrorWrongProfile) {
        break;
      }
    }
  }
}

void SessionsPrivateGetAllFunction::SendResponse(Index_Model* model) {
  namespace Results = vivaldi::sessions_private::GetAll::Results;
  using extensions::vivaldi::sessions_private::SessionItem;
  using extensions::vivaldi::sessions_private::SessionModel;

  SessionModel session_model;
  for (auto& child : model->items_node()->children()) {
    session_model.items.push_back(MakeAPITreeNode(child.get(),
                                                  model->items_node()));
  }
  session_model.root_id = Index_Node::items_node_id();
  session_model.autosave_id = Index_Node::autosave_node_id();
  session_model.trash_id = Index_Node::trash_node_id();
  session_model.loading_failed = model->loadingFailed();

  Respond(ArgumentList(Results::Create(session_model)));
}

ExtensionFunction::ResponseAction SessionsPrivateGetAutosaveIdsFunction::Run() {
  namespace Results = vivaldi::sessions_private::GetAutosaveIds::Results;
  using vivaldi::sessions_private::GetAutosaveIds::Params;
  std::optional<Params> params = Params::Create(args());

  std::vector<Index_Node*> nodes;
  sessions::GetExpiredAutoSaveNodes(browser_context(),
                                    params->days,
                                    false,
                                    nodes);
  std::vector<double> list;
  for (const Index_Node* node : nodes) {
    list.push_back(node->id());
  }
  return RespondNow(ArgumentList(Results::Create(list)));
}

ExtensionFunction::ResponseAction SessionsPrivateGetContentFunction::Run() {
  namespace Results = vivaldi::sessions_private::GetContent::Results;
  using vivaldi::sessions_private::GetContent::Params;
  using extensions::vivaldi::sessions_private::ContentModel;
  using extensions::vivaldi::sessions_private::WindowContent;
  using extensions::vivaldi::sessions_private::WorkspaceContent;
  using extensions::vivaldi::sessions_private::TabContent;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ContentModel content_model;
  NodeModel pair = GetNodeAndModel(browser_context(), params->id);
  if (!pair.first) {
    return RespondNow(ArgumentList(Results::Create(content_model)));
  }

  MakeAPIContentModel(browser_context(), pair.first, content_model);

  return RespondNow(ArgumentList(Results::Create(content_model)));
}

ExtensionFunction::ResponseAction SessionsPrivateModifyContentFunction::Run() {
  namespace Results = vivaldi::sessions_private::ModifyContent::Results;
  using vivaldi::sessions_private::ModifyContent::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  NodeModel pair = GetNodeAndModel(browser_context(), params->id);
  if (!pair.first) {
    return RespondNow(ArgumentList(Results::Create(sessions::kErrorUnknownId)));
  }

  // Casting is ok. Tab ids were casted from int32_t when setting up model
  // from where the incoming id comes from.
  std::vector<int32_t> ids;
  for (auto id : params->commands.ids) {
    ids.push_back(static_cast<int32_t>(id));
  }

  base::FilePath path = sessions::GetPathFromNode(browser_context(),
                                                  pair.first);

  bool changed = false;
  if (params->commands.quarantine.has_value() ||
      params->commands.remove.has_value()) {
    if (params->commands.quarantine.has_value()) {
      int error_code = sessions::QuarantineTabs(
        browser_context(), path, params->commands.quarantine.value(), ids);
      changed = error_code == sessions::kNoError;
    } else {
      int error_code = sessions::DeleteTabs(browser_context(), path, ids);
      if (error_code == sessions::kErrorEmpty) {
        // All tabs removed: Remove entire entry.
        error_code = sessions::DeleteSessionFile(browser_context(), pair.first);
        // Allow a missing session file when we are deleting.
        if (error_code == sessions::kErrorFileMissing) {
          error_code = sessions::kNoError;
        }
        if (error_code == sessions::kNoError) {
          pair.second->Remove(pair.first);
        }
      } else {
        changed = error_code == sessions::kNoError;
      }
    }
  } else if (params->commands.title.has_value() &&
             params->commands.ids.size() >= 1) {
    switch(params->commands.type) {
      case vivaldi::sessions_private::ContentType::kWorkspace: {
        // Workspace titles are stored in the the session index file.
        // TODO: The primary storage should be the session file itself. We then
        // have to store <id-name> (and possible an icon id) pairs in every
        // window and make sure new windows get this information.
        double id = params->commands.ids[0];
        // Make a placeholder with a copy of data of the node we want to change
        std::unique_ptr<Index_Node> node = std::make_unique<Index_Node>("", -1);
        node->Copy(pair.first);

        base::Value::List workspaces(pair.first->workspaces().Clone());
        for (auto& elm : workspaces) {
          base::Value::Dict* dict = elm.GetIfDict();
          if (dict) {
            std::optional<double> workspace_id = dict->FindDouble("id");
            if (workspace_id.value() == id) {
              // Modify workspace data and put into placeholder.
              dict->Set("name", params->commands.title.value());
              node->SetWorkspaces(std::move(workspaces));
              // Update actual node with placeholder data.
              pair.second->Change(pair.first, node.get());
              changed = true;
              break;
            }
          }
        }
        break;
      }
      case vivaldi::sessions_private::ContentType::kGroup : {
        int error_code = sessions::SetTabStackTitle(browser_context(),
          path, ids, params->commands.title.value());
        changed = error_code == sessions::kNoError;
        break;
      }
      case vivaldi::sessions_private::ContentType::kTab : {
        int error_code = sessions::SetTabTitle(browser_context(),
          path, ids[0], params->commands.title.value());
        changed = error_code == sessions::kNoError;
        break;
      }
      default:
        break;
    }
  } else if (params->commands.pin.has_value()) {
    int error_code = sessions::PinTabs(browser_context(),
      path, params->commands.pin.value(), ids);
    changed = error_code == sessions::kNoError;
  } else if (params->commands.move.has_value()) {
    if (params->commands.target.has_value()) {
      int before_tab_id =
        static_cast<int32_t>(params->commands.target.value().before_tab_id);

      std::optional<int32_t> window_id;
      if (params->commands.target.value().window_id.has_value()) {
        window_id = static_cast<int32_t>(
            params->commands.target.value().window_id.value());
      }

      int error_code = sessions::MoveTabs(browser_context(),
        path, ids, before_tab_id, window_id, params->commands.target->pinned,
        params->commands.target->group, params->commands.target->workspace);
      changed = error_code == sessions::kNoError;
    }
  } else if (params->commands.tabstack.has_value()) {
    if (params->commands.tabstack.value()) {
      if (params->commands.target.value().group.has_value()) {
        std::string group =  params->commands.target.value().group.value();
        int error_code = sessions::SetTabStack(browser_context(), path, ids,
                                               group);
        changed = error_code == sessions::kNoError;
      }
    } else {
      int error_code = sessions::SetTabStack(browser_context(), path, ids, "");
      changed = error_code == sessions::kNoError;
    }
  } else if (params->commands.window.has_value()) {
    if (params->commands.window.value() == true) {
      if (params->commands.group_aliases.has_value()) {
        std::vector<sessions::GroupAlias> group_aliases;
        for (auto& entry : params->commands.group_aliases.value()) {
          sessions::GroupAlias group_alias;
          group_alias.group = entry.group;
          group_alias.alias = entry.alias;
          group_aliases.push_back(group_alias);
        }
        int error_code = sessions::SetWindow(browser_context(), path, ids,
          group_aliases);
        changed = error_code == sessions::kNoError;
      }
    }
  } else if (params->commands.workspace.has_value()) {
    if (params->commands.workspace_state.has_value()) {
      std::vector<sessions::GroupAlias> group_aliases;
      for (auto& entry : params->commands.workspace_state.value().groups) {
        sessions::GroupAlias group_alias;
        group_alias.group = entry.group;
        group_alias.alias = entry.alias;
        group_aliases.push_back(group_alias);
      }
      int error_code = sessions::SetWorkspace(browser_context(), path, ids,
          params->commands.workspace_state.value().item.id, group_aliases);
      changed = error_code == sessions::kNoError;
    }
  }

  if (changed) {
    // Create a temporary node and init it with the node we are to change.
    std::unique_ptr<Index_Node> tmp = std::make_unique<Index_Node>("", -1);
    tmp->Copy(pair.first);
    SetNodeState(browser_context(), path, false, tmp.get());
    // A hook for workspaces. If we add a workspace we must add auxillery
    // information to the node since it is not stored in session file.
    if (params->commands.workspace.has_value() &&
        params->commands.workspace_state.has_value()) {
      const vivaldi::sessions_private::WorkspaceItem& item =
          params->commands.workspace_state.value().item;

      base::Value::List workspaces(tmp->workspaces().Clone());
      for (auto& elm : workspaces) {
        base::Value::Dict* dict = elm.GetIfDict();
        if (dict) {
          std::optional<double> id = dict->FindDouble("id");
          if (id.has_value() && id.value() == item.id) {
            dict->Set("name", item.name);
            dict->Set("icon", item.icon);
            dict->Set("emoji", item.emoji);
            break;
          }
        }
      }
      tmp->SetWorkspaces(std::move(workspaces));
    }

    // Update the existing node.
    pair.second->Change(pair.first, tmp.get());

    ContentModel content_model;
    MakeAPIContentModel(browser_context(), pair.first, content_model);

    // Send data to UI
    SessionsPrivateAPI::SendContentChanged(
      browser_context(),
      params->id,
      content_model
    );
  }

  return RespondNow(ArgumentList(Results::Create(sessions::kNoError)));
}

ExtensionFunction::ResponseAction SessionsPrivateUpdateFunction::Run() {
  namespace Results = vivaldi::sessions_private::Update::Results;
  using vivaldi::sessions_private::Update::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  NodeModel pair = GetNodeAndModel(browser_context(), params->id);
  if (!pair.first) {
    return RespondNow(ArgumentList(Results::Create(sessions::kErrorUnknownId)));
  }

  const SessionAddOptions& options = params->options;

  sessions::WriteSessionOptions ctl;
  if (options.window_id.has_value())
    ctl.window_id = options.window_id.value();
  if (options.ids.has_value())
    ctl.ids = options.ids.value();
  ctl.filename = options.filename;

  int error_code = sessions::WriteSessionFile(browser_context(), ctl);
  if (error_code == sessions::kNoError) {
    // New child of the node we are about to update. Holds backup of node.
    std::unique_ptr<Index_Node> child = std::make_unique<Index_Node>(
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        Index_Node::GetNewId());
    child->Copy(pair.first);
    child->SetContainerGuid(pair.first->guid());

    Profile* profile = Profile::FromBrowserContext(browser_context());
    bool with_workspaces =
      profile->GetPrefs()->GetBoolean(
          vivaldiprefs::kSessionsSaveAllWorkspaces);

    // Placeholder for transferring updated data to existing node.
    std::unique_ptr<Index_Node> tmp = std::make_unique<Index_Node>("", -1);
    SetNodeState(browser_context(), ctl.path, true, tmp.get());
    if (!with_workspaces) {
      base::Value::List workspaces;
      tmp->SetWorkspaces(std::move(workspaces));
    }
    tmp->SetFilename(ctl.filename);
    // Entries we do not want to modify when updating below.
    tmp->SetTitle(pair.first->GetTitle());
    tmp->SetCreateTime(pair.first->create_time());

    // Update the existing node.
    pair.second->Change(pair.first, tmp.get());
    // Add child to the node we have updated.
    pair.second->Add(std::move(child), pair.first, 0, options.owner);
  }

  return RespondNow(ArgumentList(Results::Create(error_code)));
}

ExtensionFunction::ResponseAction SessionsPrivateOpenFunction::Run() {
  using vivaldi::sessions_private::Open::Params;
  namespace Results = vivaldi::sessions_private::Open::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiBrowserWindow* window =
      VivaldiBrowserWindow::FromId(params->window_id);
  if (!window) {
    return RespondNow(Error("No such window"));
  }

  ::vivaldi::SessionOptions opts;
  opts.newWindow_ = params->options.new_window;
  opts.oneWindow_ = params->options.one_window;
  opts.withWorkspace_ = params->options.with_workspace;
  // Casting is ok. Tab ids were casted from int32_t when setting up model
  // from where the incoming id comes from.
  for (auto id : params->options.tab_ids) {
    opts.tabs_to_include_.push_back(static_cast<int32_t>(id));
  }

  int error_code = sessions::kNoError;
  NodeModel pair = GetNodeAndModel(browser_context(), params->id);
  if (pair.first) {
    error_code = sessions::Open(window->browser(), pair.first, opts);
  }
  return RespondNow(ArgumentList(Results::Create(error_code)));
}


ExtensionFunction::ResponseAction SessionsPrivateRenameFunction::Run() {
  using vivaldi::sessions_private::Rename::Params;
  namespace Results = vivaldi::sessions_private::Rename::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int error_code = sessions::kNoError;

  NodeModel pair = GetNodeAndModel(browser_context(), params->id);
  if (pair.first) {
    pair.second->SetTitle(pair.first, base::UTF8ToUTF16(params->name));
    if (pair.first->is_container()) {
      // Rename all children
      for (auto& child : pair.first->children()) {
        pair.second->SetTitle(child.get(), base::UTF8ToUTF16(params->name));
      }
      // And also any that may have been moved to trash
      Index_Node* trash_folder = FindNode(pair.second->items_node(),
                                          Index_Node::trash_node_guid());
      if (trash_folder) {
        for (auto& child : trash_folder->children()) {
          if (child->container_guid() == pair.first->guid()) {
            pair.second->SetTitle(child.get(), base::UTF8ToUTF16(params->name));
          }
        }
      }
    }
  }

  return RespondNow(ArgumentList(Results::Create(error_code)));
}


ExtensionFunction::ResponseAction SessionsPrivateMakeContainerFunction::Run() {
  using vivaldi::sessions_private::MakeContainer::Params;
  namespace Results = vivaldi::sessions_private::MakeContainer::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // This node is the one to be a new container
  NodeModel pair = GetNodeAndModel(browser_context(), params->id);
  if (!pair.first) {
    return RespondNow(ArgumentList(Results::Create(sessions::kErrorUnknownId)));
  }

  // Old container.
  Index_Node* container_node = FindNode(pair.second->items_node(),
    pair.first->container_guid());
  if (!container_node) {
    return RespondNow(ArgumentList(Results::Create(sessions::kErrorUnknownId)));
  }

  double modify_time = container_node->modify_time();

  // Swap content. id, guid, container id and children are not touched.
  pair.second->Swap(pair.first, container_node);

  // Sort the swapped child into newest modify date first.
  size_t index = 0;
  for (; index < container_node->children().size(); index++) {
    Index_Node* node = container_node->children()[index].get();
    if (modify_time > node->modify_time()) {
      break;
    }
  }
  pair.second->Move(pair.first, container_node, index);

  return RespondNow(ArgumentList(Results::Create(sessions::kNoError)));
}


ExtensionFunction::ResponseAction SessionsPrivateMoveFunction::Run() {
  using vivaldi::sessions_private::Move::Params;
  namespace Results = vivaldi::sessions_private::Move::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int error_code = sessions::kNoError;
  NodeModel pair = GetNodeAndModel(browser_context(), params->id);
  if (pair.first) {
    Index_Node* target = pair.second->root_node()->GetById(params->parent_id);
    size_t index = params->index;
    // ALl children of a container are sorted by newest modify date first.
    if (target->is_container()) {
      for (index = 0; index < target->children().size(); index++) {
        Index_Node* node = target->children()[index].get();
        if (node->modify_time() < pair.first->modify_time()) {
          break;
        }
      }
    }
    pair.second->Move(pair.first, target, index);
  }

  return RespondNow(ArgumentList(Results::Create(error_code)));
}


ExtensionFunction::ResponseAction SessionsPrivateDeleteFunction::Run() {
  using vivaldi::sessions_private::Delete::Params;
  namespace Results = vivaldi::sessions_private::Delete::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int error_code = sessions::kNoError;
  NodeModel pair = GetNodeAndModel(browser_context(), params->id);
  if (pair.first) {
    if (pair.second->IsTrashed(pair.first)) {
      error_code = sessions::DeleteSessionFile(browser_context(), pair.first);
      // Allow a missing session file when we are deleting.
      if (error_code == sessions::kErrorFileMissing) {
        error_code = sessions::kNoError;
      }
      if (error_code == sessions::kNoError) {
        pair.second->Remove(pair.first);
      }
    } else {
      Index_Node* target = pair.second->root_node()->GetById(
          Index_Node::trash_node_id());
      pair.second->Move(pair.first, target, params->index);
    }
  }

  return RespondNow(ArgumentList(Results::Create(error_code)));
}

ExtensionFunction::ResponseAction SessionsPrivateEmptyTrashFunction::Run() {
  namespace Results = vivaldi::sessions_private::EmptyTrash::Results;
  int error_code = sessions::kNoError;

  NodeModel pair = GetNodeAndModel(browser_context(),
                                   Index_Node::trash_node_id());

  if (pair.first) {
    for (size_t count = pair.first->children().size(); count > 0; count--) {
      auto& child =  pair.first->children().at(0);
      error_code = sessions::DeleteSessionFile(browser_context(), child.get());
      // Allow a missing session file when we are deleting.
      if (error_code == sessions::kErrorFileMissing) {
        error_code = sessions::kNoError;
      }
      if (error_code == sessions::kNoError) {
        pair.second->Remove(child.get());
      } else {
        break;
      }
    }
  }

  return RespondNow(ArgumentList(Results::Create(error_code)));
}

ExtensionFunction::ResponseAction
SessionsPrivateRestoreLastClosedFunction::Run() {
  namespace Results = vivaldi::sessions_private::RestoreLastClosed::Results;
  bool did_open_session_window = false;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  SessionService* session_service =
      SessionServiceFactory::GetForProfileForSessionRestore(
          profile->GetOriginalProfile());
  if (session_service &&
      session_service->RestoreIfNecessary(StartupTabs(),
                                           /* restore_apps */ false)) {
    did_open_session_window = true;
  }

  return RespondNow(ArgumentList(Results::Create(did_open_session_window)));
}


}  // namespace extensions
