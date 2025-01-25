// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.

#include "browser/sessions/vivaldi_session_utils.h"

#include "app/vivaldi_constants.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "browser/sessions/vivaldi_session_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/panel/panel_id.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/vivaldi_session_service_commands.h"
#include "extensions/api/sessions/vivaldi_sessions_api.h"
#include "extensions/api/tabs/tabs_private_api.h"
#include "sessions/index_codec.h"
#include "sessions/index_model.h"
#include "sessions/index_node.h"
#include "sessions/index_service_factory.h"
#include "sessions/index_storage.h"
#include "base/uuid.h"
#include "vivaldi/app/grit/vivaldi_native_strings.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserContext;
using sessions::Index_Model;
using sessions::Index_Node;
using sessions::IndexServiceFactory;

namespace {

const int kNumberBufferSize = 16;

const char kVivaldiTabFlag[] = "flag";
const char kVivaldiTabStackId[] = "group";
const char kVivaldiTabStackTitles[] = "groupTitles";
const char kVivaldiWorkspace[] = "workspaceId";
const char kVivaldiFixedTitle[] = "fixedTitle";
const char kVivaldiFixedGroupTitle[] = "fixedGroupTitle";

#define TAB_QUARANTNE 0x01

bool SetTabFlag(sessions::SessionTab* tab, int key, bool flag) {
  std::string ext_data = tab->viv_ext_data;
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> json =
     base::JSONReader::Read(ext_data, options);
  if (json && json->is_dict()) {
    std::optional<int> candidate = json->GetDict().FindDouble(kVivaldiTabFlag);
    int value = candidate.has_value() ? candidate.value() : 0;
    if (flag) {
      value |= key;
    } else {
      value &= ~key;
    }
    json->GetDict().Set(kVivaldiTabFlag, value);
    std::string modified;
    base::JSONWriter::Write(base::ValueView(json.value()), &modified);
    tab->viv_ext_data = modified;
    return true;
  }
  return false;
}

std::optional<int> GetTabFlag(const sessions::SessionTab* tab) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> json =
     base::JSONReader::Read(tab->viv_ext_data, options);
  std::optional<int> value;
  if (json && json->is_dict()) {
    value = json->GetDict().FindInt(kVivaldiTabFlag);
  }
  return value;
}

bool SetTabStackId(sessions::SessionTab* tab, const std::string& id) {
  std::string ext_data = tab->viv_ext_data;
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> json =
    base::JSONReader::Read(ext_data, options);
  std::string value;
  if (json && json->is_dict()) {
    json->GetDict().Set(kVivaldiTabStackId, id);
    std::string modified;
    base::JSONWriter::Write(base::ValueView(json.value()), &modified);
    tab->viv_ext_data = modified;
    return true;
  }
  return false;
}

bool RemoveTabStackId(sessions::SessionTab* tab) {
  std::string ext_data = tab->viv_ext_data;
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> json =
    base::JSONReader::Read(ext_data, options);
  std::string value;
  if (json && json->is_dict()) {
    json->GetIfDict()->Remove(kVivaldiTabStackId);
    std::string modified;
    base::JSONWriter::Write(base::ValueView(json.value()), &modified);
    tab->viv_ext_data = modified;
    return true;
  }
  return false;
}

bool RemoveTabStackTitle(sessions::SessionWindow* window, std::string group) {
  std::string ext_data = window->viv_ext_data;
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> json =
    base::JSONReader::Read(ext_data, options);
  if (json && json->is_dict()) {
    base::Value::Dict* titles = json->GetIfDict()->FindDict(
        kVivaldiTabStackTitles);
    if (titles) {
      titles->Remove(group);
      if (titles->empty()) {
        json->GetIfDict()->Remove(kVivaldiTabStackTitles);
      } else {
        json->GetIfDict()->Set(kVivaldiTabStackTitles, std::move(*titles));
      }
      std::string modified;
      base::JSONWriter::Write(base::ValueView(json.value()), &modified);
      window->viv_ext_data = modified;
      return true;
    }
  }
  return false;
}

std::optional<double> GetWorkspaceId(sessions::SessionTab* tab) {
  std::string ext_data = tab->viv_ext_data;
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> json =
      base::JSONReader::Read(ext_data, options);
  std::optional<double> value;
  if (json && json->is_dict()) {
    value = json->GetDict().FindDouble(kVivaldiWorkspace);
  }
  return value;
}

bool SetWorkspaceId(sessions::SessionTab* tab, double id) {
  std::string ext_data = tab->viv_ext_data;
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> json =
    base::JSONReader::Read(ext_data, options);
  std::string value;
  if (json && json->is_dict()) {
    json->GetDict().Set(kVivaldiWorkspace, id);
    std::string modified;
    base::JSONWriter::Write(base::ValueView(json.value()), &modified);
    tab->viv_ext_data = modified;
    return true;
  }
  return false;
}

bool RemoveWorkspaceId(sessions::SessionTab* tab) {
  std::string ext_data = tab->viv_ext_data;
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> json =
    base::JSONReader::Read(ext_data, options);
  std::string value;
  if (json && json->is_dict()) {
    json->GetIfDict()->Remove(kVivaldiWorkspace);
    std::string modified;
    base::JSONWriter::Write(base::ValueView(json.value()), &modified);
    tab->viv_ext_data = modified;
    return true;
  }
  return false;
}

// Returns true when 'ids' refers to one or more tabs in the browser.
bool ContainsTabs(Browser* browser, const std::vector<int>& ids) {
  bool has_content = false;
  TabStripModel* tab_strip = browser->tab_strip_model();
  for (int i = 0; i < tab_strip->count(); ++i) {
    content::WebContents* tab = tab_strip->GetWebContentsAt(i);
    DCHECK(tab);
    if (!ids.empty()) {
      int id = extensions::ExtensionTabUtil::GetTabId(tab);
      for (unsigned j = 0; j < ids.size(); j++) {
        if (ids.at(j) == id) {
          has_content = true;
          break;
        }
      }
    }
  }
  return has_content;
}

// This controller collects information of stacks in the set of tabs and
// remove tabs from stacks if there are too few tabs to form a stack.
class StackController {
 public:
  // Starts tracking the group id of the tab.
  void track(sessions::SessionTab* tab, const std::string& group = "") {
    std::string id = GetTabStackId(tab);
    if (!id.empty() && (group.empty() || id != group)) {
      if (groups_.find(id) == groups_.end()) {
        groups_[id] = std::vector<sessions::SessionTab*>();
      }
    }
  }
  // Adds a tab to the proper tracking list if present.
  void append(sessions::SessionTab* tab) {
    std::string id = GetTabStackId(tab);
    if (!id.empty() && groups_.find(id) != groups_.end()) {
      groups_[id].push_back(tab);
    }
  }
  // Remove group info from tabs (and thereby remove tabs from stacks) if there
  // are less than two tabs in the tracked list.
  void purge(std::vector<std::unique_ptr<sessions::SessionWindow>>& windows) {
    for (auto it = groups_.begin(); it != groups_.end(); ++it) {
      std::vector<sessions::SessionTab*>& tabs = it->second;
      if (tabs.size() < 2) {
        for (sessions::SessionTab* tab : tabs) {
          RemoveTabStackId(tab);
        }
        for (size_t i = 0; i < windows.size(); i++) {
          RemoveTabStackTitle(windows[i].get(), it->first);
        }
      }
    }
  }
 private:
  std::map<std::string, std::vector<sessions::SessionTab*>> groups_;
};



base::FilePath MakePath(BrowserContext* browser_context, std::string seed,
                        std::string* filename) {
  // PathExists() triggers IO restriction.
  base::VivaldiScopedAllowBlocking allow_blocking;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  std::string temp_seed = seed;
  char number_string[kNumberBufferSize];

  // Avoid any endless loop, which is highly unlikely, but still.
  for (int i = 2; i < 1000; i++) {
    base::FilePath path = base::FilePath(profile->GetPath())
      .Append(sessions::IndexStorage::GetFolderName())
#if BUILDFLAG(IS_POSIX)
      .Append(temp_seed)
#elif BUILDFLAG(IS_WIN)
      .Append(base::UTF8ToWide(temp_seed))
#endif
      .AddExtension(FILE_PATH_LITERAL(".bin"));
    if (base::PathExists(path)) {
      base::snprintf(number_string, kNumberBufferSize, " (%d)", i);
      temp_seed = seed;
      temp_seed.append(number_string);
    } else {
      *filename = temp_seed + ".bin";
      return path;
    }
  }

  NOTREACHED();
  //return base::FilePath();
}

int CopySessionFile(BrowserContext* browser_context,
                    sessions::WriteSessionOptions& opts) {
  // PathExists() triggers IO restriction.
  base::VivaldiScopedAllowBlocking allow_blocking;

  Index_Model* model = IndexServiceFactory::GetForBrowserContext(
      browser_context);
  Index_Node* node = model && model->items_node()
    ? model->items_node()->GetById(opts.from_id)
    : nullptr;
  if (!node && opts.from_id == Index_Node::backup_node_id()) {
    // Fallback for the case we deal with the internal kBackupNodeId.
    node = model && model->root_node()
      ? model->root_node()->GetById(opts.from_id)
      : nullptr;
  }

  if (!node) {
    return sessions::kErrorUnknownId;
  }

  if (opts.filename.empty()) {
    return sessions::kErrorMissingName;
  }

  base::FilePath path = sessions::GetPathFromNode(browser_context, node);
  if (!base::PathExists(path)) {
    return sessions::kErrorFileMissing;
  }

  opts.path = MakePath(browser_context, opts.filename, &opts.filename);
  if (opts.path.empty()) {
    return sessions::kErrorFileMissing;
  }

  if (!base::CopyFile(path, opts.path)) {
    return sessions::kErrorFileCopyFailure;
  }

  return sessions::kNoError;
}

int PurgeAutosaves(BrowserContext* browser_context) {
  Index_Model* model = IndexServiceFactory::GetForBrowserContext(
      browser_context);
  if (!model || !model->items_node()) {
    return sessions::kErrorNoModel;
  }

  Index_Node* autosave_node = model->items_node()->GetById(
      Index_Node::autosave_node_id());
  if (!autosave_node) {
    return sessions::kNoError;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  int save_days = profile->GetPrefs()->GetInteger(
      vivaldiprefs::kSessionsSaveDays);

  std::vector<Index_Node*> nodes;
  sessions::GetExpiredAutoSaveNodes(browser_context, save_days, true, nodes);

  for (Index_Node* node : nodes) {
    int error_code = DeleteSessionFile(browser_context, node);
    // Allow a missing session file when we are deleting.
    if (error_code != sessions::kNoError &&
        error_code != sessions::kErrorFileMissing) {
      return error_code;
    }
    model->Remove(node);
  }
  return sessions::kNoError;
}


int HandleAutoSave(BrowserContext* browser_context,
                   sessions::WriteSessionOptions& ctl,
                   double* modify_time = nullptr) {
  Index_Model* model = IndexServiceFactory::GetForBrowserContext(
      browser_context);
  if (!model || !model->items_node()) {
    return sessions::kErrorNoModel;
  }

  int error_code = WriteSessionFile(browser_context, ctl);
  if (error_code != sessions::kNoError) {
    return error_code;
  }

  Index_Node* autosave_node = model->items_node()->GetById(
      Index_Node::autosave_node_id());
  if (!autosave_node) {
    std::unique_ptr<Index_Node> node = std::make_unique<Index_Node>(
        Index_Node::autosave_node_guid(), Index_Node::autosave_node_id());
    node->SetTitle(l10n_util::GetStringUTF16(IDS_VIV_SESSION_AUTOSAVE_FOLDER));
    node->SetFilename(ctl.filename); // Must be set if calling DeleteSessionFile

    error_code = SetNodeState(browser_context, ctl.path, true, node.get());
    if (error_code != sessions::kNoError) {
      DeleteSessionFile(browser_context, node.get());
      return error_code;
    }
    // See explanation a bit down in function. Since this is the first auto save
    // node we also set the create time.
    if (modify_time) {
      node->SetCreateTime(*modify_time);
      node->SetModifyTime(*modify_time);
    }

    size_t index = model->items_node()->children().size();
    if (index > 0) {
      if (model->items_node()->children()[index - 1]->is_trash_folder()) {
        index --;
      }
    }
    model->Add(std::move(node), model->items_node(), index, "");
  } else {
    // When we add a new child to the autosave_node we do it by making the
    // auto_save node itself the node that holds the new data. The existing
    // content of the auto_save node is added as the first child. This way we
    // "push" the older state down into child list forming a history.

    // The new child of the auto_save node we are about to update. Holds old
    // state of the auto_save node.
    std::unique_ptr<Index_Node> child = std::make_unique<Index_Node>(
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        Index_Node::GetNewId());
    child->Copy(autosave_node);
    child->SetContainerGuid(autosave_node->guid());

    // Placeholder for transferring new data to the auto_save node.
    std::unique_ptr<Index_Node> tmp = std::make_unique<Index_Node>("", -1);
    tmp->SetFilename(ctl.filename); // Must be set if calling DeleteSessionFile

    error_code = SetNodeState(browser_context, ctl.path, true, tmp.get());
    if (error_code != sessions::kNoError) {
      DeleteSessionFile(browser_context, tmp.get());
      return error_code;
    }
    // Entries we want to keep unmodified when updating the autosave_node below.
    tmp->SetTitle(autosave_node->GetTitle());
    tmp->SetCreateTime(autosave_node->create_time());
    // The typical case for overriding the modify time is when we update the
    // auto save list with a backup file while loading the model. The code above
    // (WriteSessionFile) will then do a file copy operation casing the modify
    // time to be 'now'. That time should be when the backup got created.
    if (modify_time) {
      tmp->SetModifyTime(*modify_time);
    }

    // Update auto_save node with newest save state.
    model->Change(autosave_node, tmp.get());
    // Add child with old autosave_node data as first child of autosave_node.
    model->Add(std::move(child), autosave_node, 0, "");
  }

  // Remove too old nodes.
  PurgeAutosaves(browser_context);

  // Always delete any backup when auto save list is updated.
  if (model->backup_node()) {
    DeleteSessionFile(browser_context, model->backup_node());
    model->Remove(model->backup_node());
  }

  return error_code;
}


int HandlePersistentSave(BrowserContext* browser_context,
                         sessions::WriteSessionOptions& ctl) {
  Index_Model* model = IndexServiceFactory::GetForBrowserContext(
      browser_context);
  if (!model) {
    return sessions::kErrorNoModel;
  }

  int id = Index_Node::persistent_node_id();

  Index_Node* old_node = model->root_node()->GetById(id);
  if (old_node) {
    DeleteSessionFile(browser_context, old_node);
    model->Remove(old_node);
  }

  int error_code = sessions::WriteSessionFile(browser_context, ctl);
  if (error_code == sessions::kNoError) {
    std::unique_ptr<Index_Node> node = std::make_unique<Index_Node>(
        Index_Node::persistent_node_guid(), id);
    SetNodeState(browser_context, ctl.path, true, node.get());
    node->SetFilename(ctl.filename);
    model->Add(std::move(node), model->root_node(), 0, "");
  }

  return sessions::kNoError;
}


void SortTabs(std::vector<std::unique_ptr<sessions::SessionTab>>& tabs) {
  std::sort(tabs.begin(), tabs.end(),
            [](std::unique_ptr<sessions::SessionTab>& tab1,
               std::unique_ptr<sessions::SessionTab>& tab2) {
              return tab1->tab_visual_index < tab2->tab_visual_index;
            });

}

}  // namespace

namespace sessions {

WriteSessionOptions::WriteSessionOptions() {}
WriteSessionOptions::~WriteSessionOptions() {}

SessionContent::SessionContent() {}
SessionContent::~SessionContent() {}


int Open(Browser* browser, Index_Node* node,
         const ::vivaldi::SessionOptions& opts) {
  base::VivaldiScopedAllowBlocking allow_blocking;

  base::FilePath path = GetPathFromNode(browser->profile(), node);
  ::vivaldi::VivaldiSessionService service(browser->profile());
  return base::PathExists(path)
    ? service.Load(path, browser, opts)
    : kErrorFileMissing;
}

int OpenPersistentTabs(Browser* browser, bool discard) {
  Index_Model* model = IndexServiceFactory::GetForBrowserContext(
      browser->profile());
  if (!model) {
    return kErrorNoModel;
  }

  // Only regular windows can save and open persistent tabs.
  if (browser->profile()->IsGuestSession() ||
      browser->profile()->IsOffTheRecord()) {
    return kErrorWrongProfile;
  }

  Index_Node* node = model->root_node()->GetById(
      Index_Node::persistent_node_id());
  if (node) {
    if (discard) {
      DeleteSessionFile(browser->profile(), node);
      model->Remove(node);
    } else {
      extensions::SessionsPrivateAPI::SendOnPersistentLoad(browser->profile(),
        true);
      vivaldi::SessionOptions opts;
      opts.newWindow_ = false;
      opts.oneWindow_ = true;
      opts.withWorkspace_ = true;
      Open(browser, node, opts);
      // Can only be opened once after it has been saved so deleting now.
      DeleteSessionFile(browser->profile(), node);
      model->Remove(node);
      extensions::SessionsPrivateAPI::SendOnPersistentLoad(browser->profile(),
        false);
    }
  }
  return kNoError;
}

base::FilePath GetPathFromNode(BrowserContext* browser_context,
                               Index_Node* node) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return base::FilePath(profile->GetPath())
    .Append(sessions::IndexStorage::GetFolderName())
#if BUILDFLAG(IS_POSIX)
    .Append(node->filename());
#elif BUILDFLAG(IS_WIN)
    .Append(base::UTF8ToWide(node->filename()));
#endif
}

int SetNodeState(BrowserContext* browser_context, const base::FilePath& file,
                 bool is_new, Index_Node* node) {
  base::VivaldiScopedAllowBlocking allow_blocking;
  // Get time from file. Same as when setting up the model on first run.
  base::File::Info info;
  if (!base::GetFileInfo(file, &info)) {
    return kErrorMissingName;
  }

  base::Time created = info.creation_time;
  node->SetCreateTime(created.InMillisecondsFSinceUnixEpoch());
  node->SetModifyTime(is_new ? created.InMillisecondsFSinceUnixEpoch()
                          : info.last_modified.InMillisecondsFSinceUnixEpoch());

  sessions::SessionContent content;
  sessions::GetContent(file, content);

  int num_windows = content.windows.size();
  int num_tabs = content.tabs.size();
  if (num_windows == 0 || num_tabs == 0) {
    return kErrorNoContent;
  }
  node->SetWindowsCount(num_windows);
  node->SetTabsCount(num_tabs);

  Profile* profile = Profile::FromBrowserContext(browser_context);

  // Update quarantined and workspace state

  base::Value::List workspaces;
  if (is_new) {
    // For a new node it is simple. There can be no quarantined nodes and all
    // workspaces can be tagged active (meaning UI code will show them, we make
    // a workspace inactive if all tabs inside are quarantined - that way we
    // we keep icons and name in case we remove quarantine state later on).
    base::Value::List known_workspaces(
        profile->GetPrefs()->GetList(vivaldiprefs::kWorkspacesList).Clone());
    for (base::Value& elm : known_workspaces) {
      base::Value::Dict* dict = elm.GetIfDict();
      if (dict) {
        dict->Set("active", true);
        workspaces.Append(dict->Clone());
      }
    }
    node->SetWorkspaces(workspaces.Clone());
  } else {
    // Ext-data in tabs may have updated.
    std::vector<double> listed; // Workspaces that tabs refer to.
    std::vector<double> active; // Workspaces that active (non-quarantined) tabs belong to.
    std::vector<double>::iterator it;
    int num_quarantine = 0;
    for (auto tit = content.tabs.begin(); tit != content.tabs.end(); ++tit) {
      bool is_quarantine = sessions::IsTabQuarantined(tit->second.get());
      if (is_quarantine) {
        num_quarantine ++;
      }
      std::optional<double> id = GetWorkspaceId(tit->second.get());
      if (id.has_value()) {
        it = std::find(listed.begin(), listed.end(), id.value());
        if (it == listed.end()) {
          listed.push_back(id.value());
        }
        if (!is_quarantine) {
          it = std::find(active.begin(), active.end(), id.value());
          if (it == active.end()) {
            active.push_back(id.value());
          }
        }
      }
    }
    node->SetQuarantineCount(num_quarantine);

    // Add all workspaces that are known to node if present in tabs.
    base::Value::List known_workspaces(node->workspaces().Clone());
    for (base::Value& elm : known_workspaces) {
      base::Value::Dict* dict = elm.GetIfDict();
      if (dict) {
        std::optional<double> id = dict->FindDouble("id");
        if (id.has_value()) {
          it = std::find(listed.begin(), listed.end(), id.value());
          if (it != listed.end()) {
            listed.erase(it);
            base::Value::Dict entry(dict->Clone());
            it = std::find(active.begin(), active.end(), id.value());
            entry.Set("active", it != active.end());
            workspaces.Append(entry.Clone());
          }
        }
      }
    }
    // Add all workspaces (new workspaces) known in tabs, but not in node.
    if (listed.size() > 0) {
      for (auto& id : listed) {
        base::Value::Dict entry;
        entry.Set("id", id);
        it = std::find(active.begin(), active.end(), id);
        entry.Set("active", it != active.end());
        workspaces.Append(entry.Clone());
      }
    }
  }
  node->SetWorkspaces(workspaces.Clone());

  // With VB-23686 we save tab and tab stack titles to tab ext data in JS. So
  // we no longer save to the separate group member here.
  base::Value::Dict all_groups_names;
  node->SetGroupNames(std::move(all_groups_names));

  return kNoError;
}

std::vector<std::string> CollectThumbnailUrls(
    content::BrowserContext* browser_context,
    const WriteSessionOptions& opts) {
  std::vector<std::string> res;
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ::vivaldi::VivaldiSessionService service(profile);
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (service.ShouldTrackWindow(browser) &&
        browser->tab_strip_model()->count() && browser->window() &&
        // An empty list means all tabs.
        (opts.ids.size() == 0 || ContainsTabs(browser, opts.ids)) &&
        // A window id of 0 means all windows.
        (opts.window_id == 0 ||
         (browser->session_id().id() == opts.window_id))) {
      std::vector<std::string> urls =
          service.CollectThumbnailUrls(browser, opts.ids);
      res.insert(res.end(), urls.begin(), urls.end());
    }
  }

  return res;
}

std::vector<std::string> CollectAllThumbnailUrls(
    content::BrowserContext* browser_context) {
  std::vector<std::string> res;
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ::vivaldi::VivaldiSessionService service(profile);
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->window()) {
      auto thumbnails =
          service.CollectThumbnailUrls(browser, std::vector<int>());
      res.insert(std::end(res), std::begin(thumbnails), std::end(thumbnails));
    }
  }

  return res;
}

int WriteSessionFile(content::BrowserContext* browser_context,
                     WriteSessionOptions& opts) {
  if (opts.from_id != -1) {
    return CopySessionFile(browser_context, opts);
  }

  if (opts.filename.empty()) {
    return kErrorMissingName;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  opts.path = MakePath(browser_context, opts.filename, &opts.filename);
  if (opts.path.empty()) {
    return kErrorFileMissing;
  }

  ::vivaldi::VivaldiSessionService service(profile);
  for (Browser* browser : *BrowserList::GetInstance()) {
    // 1 The tracking test will prevent saving content in private (or guest)
    //   windows from regular (but private can save regular).
    // 2 Make sure the browser has tabs and a window. Browser's destructor
    //   removes itself from the BrowserList. When a browser is closed the
    //   destructor is not necessarily run immediately. This means it's
    //   possible for us to get a handle to a browser that is about to be
    //   removed. If the tab count is 0 or the window is NULL, the browser
    //   is about to be deleted, so we ignore it.
    if (service.ShouldTrackWindow(browser) &&
        browser->tab_strip_model()->count() && browser->window() &&
        // An empty list means all tabs.
        (opts.ids.size() == 0 || ContainsTabs(browser, opts.ids)) &&
        // A window id of 0 means all windows.
        (opts.window_id == 0 ||
         (browser->session_id().id() == opts.window_id))) {
      service.BuildCommandsForBrowser(browser, opts.ids, opts.thumbnails);
    }
  }
  if (!service.Save(opts.path)) {
    return kErrorWriting;
  }

  return kNoError;
}

int DeleteSessionFile(BrowserContext* browser_context, Index_Node* node) {
  base::VivaldiScopedAllowBlocking allow_blocking;
  base::FilePath path = GetPathFromNode(browser_context, node);
  if (!base::PathExists(path)) {
    return kErrorFileMissing;
  }

  if (!base::DeleteFile(path)) {
    return kErrorDeleteFailure;
  }

  return kNoError;
}

int MoveAutoSaveNodesToTrash(BrowserContext* browser_context) {
  Index_Model* model = IndexServiceFactory::GetForBrowserContext(
      browser_context);
  if (!model || !model->items_node()) {
    return sessions::kErrorNoModel;
  }

  Index_Node* autosave_node = model->items_node()->GetById(
      Index_Node::autosave_node_id());
  if (!autosave_node) {
    return sessions::kNoError;
  }

  // Move all children of node to trash.
  std::vector<Index_Node*> nodes;
  for (const std::unique_ptr<Index_Node>& node : autosave_node->children()) {
    nodes.push_back(node.get());
  }
  Index_Node* trash_folder = model->root_node()->GetById(
      Index_Node::trash_node_id());
  if (!trash_folder) {
    return sessions::kErrorUnknownId;
  }

  int index = 0;
  for (const Index_Node* node : nodes) {
    model->Move(node, trash_folder, index++);
  }

  // Move node itself to trash. We can not move as is since it holds a
  // special id. Duplicate content with new id.
  std::unique_ptr<Index_Node> node = std::make_unique<Index_Node>(
    base::Uuid::GenerateRandomV4().AsLowercaseString(),
    Index_Node::GetNewId());
  node->Copy(autosave_node);
  model->Add(std::move(node), trash_folder, 0, "");
  model->Remove(autosave_node);

  return kNoError;
}

// Adds children of the autosave main node to the 'nodes' list of too old nodes.
// All nodes made on the same date as autosave main node are added if 'on_add'
// is true.
int GetExpiredAutoSaveNodes(BrowserContext* browser_context,
                            int days_before,
                            bool on_add,
                            std::vector<Index_Node*>& nodes) {
  Index_Model* model = IndexServiceFactory::GetForBrowserContext(
      browser_context);
  if (!model || !model->items_node()) {
    return sessions::kErrorNoModel;
  }

  Index_Node* autosave_node = model->items_node()->GetById(
      Index_Node::autosave_node_id());
  if (!autosave_node) {
    return sessions::kNoError;
  }

  base::Time time = on_add ? base::Time::FromMillisecondsSinceUnixEpoch(
                                 autosave_node->modify_time())
    : base::Time::Now();
  base::Time::Exploded time_exploded;
  time.LocalExplode(&time_exploded);

  // First add all from the same date. The top node is the only one that shall
  // hold data from current date.
  if (on_add) {
    for (const std::unique_ptr<Index_Node>& node : autosave_node->children()) {
      base::Time node_time =
        base::Time::FromMillisecondsSinceUnixEpoch(node->modify_time());
      base::Time::Exploded node_exploded;
      node_time.LocalExplode(&node_exploded);
      if (time_exploded.year == node_exploded.year &&
          time_exploded.day_of_month == node_exploded.day_of_month &&
          time_exploded.month == node_exploded.month) {
        nodes.push_back(node.get());
      }
    }
  }
  // Next, add entries so that when we remove them the number of children of
  // the autosave node does not exceed the value of days_before.
  // This assumes the list is sorted by date (oldest last).
  if (autosave_node->children().size() - nodes.size() > static_cast<size_t>(days_before)) {
    int num_to_remove = autosave_node->children().size() -
      nodes.size() - days_before;
    for (int i = autosave_node->children().size() - 1;
         i >= 0 && num_to_remove > 0;
         --i) {
      const std::unique_ptr<Index_Node>& node = autosave_node->children().at(i);
      bool can_add = true;
      for (size_t j=0; j < nodes.size() && can_add; j++) {
        if (nodes.at(j) == node.get()) {
          can_add = false;
        }
      }
      if (can_add) {
        nodes.push_back(node.get());
        num_to_remove--;
      } else {
      }
    }
  }

  return sessions::kNoError;
}

// Moves a backup session to the auto saved session list. Intended to be used
// while loading the model.
int AutoSaveFromBackup(BrowserContext* browser_context) {
  Index_Model* model = IndexServiceFactory::GetForBrowserContext(
      browser_context);
  if (!model || !model->items_node() || !model->backup_node()) {
    return sessions::kErrorNoModel;
  }

  // If auto saving is turned off backup generation is also turned off. Should
  // there be backup information in the model when loading we just delete it
  // since the user has turned functionality off.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile->GetPrefs()->GetInteger(vivaldiprefs::kSessionsSaveOnExit) != 2) {
    DeleteSessionFile(browser_context, model->backup_node());
    model->Remove(model->backup_node());
    return sessions::kNoError;
  }

  // Moves the backup node / file to the head of the auto save list.
  WriteSessionOptions ctl;
  ctl.filename = "autosave";
  ctl.from_id = Index_Node::backup_node_id();
  double create_time = model->backup_node()->create_time();
  return HandleAutoSave(browser_context, ctl, &create_time);
}

void AutoSave(BrowserContext* browser_context, bool from_ui) {
  // Guard. We attempt to save the session when profile manager shuts down
  // ('from_ui' is false) or from UI when we have accepcted a confirmation
  // dialog or when we exit from a menu without the dialog. The UI code dialog
  // gets executed after the profile manager hook (the profile mananger is
  // reborn if we cancel exit from the dialog). In order not to save too early
  // (we can cancel exit from the dialog) we test here if ok to proceed.
  if (!from_ui) {
#if !BUILDFLAG(IS_MAC)
    Profile* profile = Profile::FromBrowserContext(browser_context);
    if (profile->GetPrefs()->GetBoolean(
        vivaldiprefs::kSystemShowExitConfirmationDialog)) {
      // Ignore: Not from UI and we will save when dialog is accecpted.
      return;
    }
#endif
  }

  // Guard. This function shall save once in the program's lifetime to prevent
  // multiple entries set up on exit.
  static bool has_been_called = false;
  if (has_been_called) {
    return;
  }
  has_been_called = true;

  Index_Model* model = IndexServiceFactory::GetForBrowserContext(
      browser_context);
  if (!model || !model->items_node()) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile->GetPrefs()->GetInteger(vivaldiprefs::kSessionsSaveOnExit) != 2) {
    return;
  }

  WriteSessionOptions ctl;
  ctl.filename = "autosave";
  HandleAutoSave(browser_context, ctl);
}

int SavePersistentTabs(BrowserContext* browser_context, std::vector<int> ids) {
  Index_Model* model = IndexServiceFactory::GetForBrowserContext(
      browser_context);
  if (!model || !model->items_node()) {
    return kErrorNoModel;
  }

  WriteSessionOptions ctl;
  ctl.filename = "persistent";
  ctl.ids = ids;
  return HandlePersistentSave(browser_context, ctl);
}

int DeleteTabs(BrowserContext* browser_context,
               base::FilePath path,
               std::vector<int32_t> ids) {

  // Load content
  SessionContent content;
  GetContent(path, content);

  // Move content except those tabs that shall be removed into arrays.
  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  std::vector<std::unique_ptr<sessions::SessionTab>> tabs;
  // We may want to remove group state from remaining tabs.
  std::map<std::string, std::vector<sessions::SessionTab*>> groups;
  for (auto it=content.windows.begin(); it!=content.windows.end(); ++it) {
    windows.push_back(std::move(it->second));
  }
  for (auto it = content.tabs.begin(); it != content.tabs.end(); ++it) {
    if (std::find(ids.begin(), ids.end(), it->second->tab_id.id()) ==
        ids.end()) {
      tabs.push_back(std::move(it->second));
    } else {
      std::string group = GetTabStackId(it->second.get());
      if (!group.empty()) {
        // Removed tab is part of a group (tab stack). Save group so that we can
        // remove group state from remaning tabs, if needed.
        auto git = groups.find(group);
        if (git == groups.end()) {
          std::vector<sessions::SessionTab*> vec;
          groups[group] = vec;
        }
      }
    }
  }
  if (tabs.size() == 0) {
    // We are about to remove all tabs. Let caller handle this.
    return kErrorEmpty;
  }

  if (groups.size() > 0) {
    // We have removed one or more tabs belonging to groups. Test all remaning
    // tabs and add those that match a removed group to the list.
    for (auto& tab : tabs) {
      std::string group = GetTabStackId(tab.get());
      if (!group.empty()) {
        auto git = groups.find(group);
        if (git != groups.end()) {
          git->second.push_back(tab.get());
        }
      }
    }
    // If there is only one tab in a group list it means there is a group of
    // two or more and we are about to remove all but one. We must then remove
    // group information from the one that remains as well.
    for (auto git = groups.begin(); git != groups.end(); ++git) {
      if (git->second.size() == 1) {
        RemoveTabStackId(git->second[0]);
        // Also, remove any custom tab stack title if set.
        for (size_t i = 0; i < windows.size(); i++) {
          if (windows[i]->window_id == git->second[0]->window_id) {
            RemoveTabStackTitle(windows[i].get(), git->first);
            break;
          }
        }
      }
    }
  }

  // Build a command set.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ::vivaldi::VivaldiSessionService service(profile);
  bool has_commands = service.SetCommands(windows, tabs) > 0;
  if (!has_commands) {
    // We have tabs but no content to write.
    return kErrorNoContent;
  }

  // And save the updated set.
  if (!service.Save(path)) {
    return kErrorWriting;
  }

  return kNoError;
}

int PinTabs(BrowserContext* browser_context,
            base::FilePath path,
            bool value,
            std::vector<int32_t> ids) {
  // Load content
  SessionContent content;
  GetContent(path, content);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  std::vector<std::unique_ptr<sessions::SessionTab>> tabs;
  for (auto it=content.windows.begin(); it!=content.windows.end(); ++it) {
    windows.push_back(std::move(it->second));
  }
  for (auto it = content.tabs.begin(); it != content.tabs.end(); ++it) {
    auto id_it = std::find(ids.begin(), ids.end(), it->second->tab_id.id());
    if (id_it != ids.end()) {
      it->second.get()->pinned = value;
    }
    tabs.push_back(std::move(it->second));
  }

  // Build a command set.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ::vivaldi::VivaldiSessionService service(profile);
  bool has_commands = service.SetCommands(windows, tabs) > 0;
  if (has_commands) {
    if (!service.Save(path)) {
      return kErrorWriting;
    }
  }

  return kNoError;
}

int MoveTabs(BrowserContext* browser_context,
             base::FilePath path,
             std::vector<int32_t> ids,
             int before_tab_id,
             std::optional<int32_t> window_id,
             std::optional<bool> pinned,
             std::optional<std::string> group,
             std::optional<double> workspace) {
  // Load content
  SessionContent content;
  GetContent(path, content);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  std::vector<std::unique_ptr<sessions::SessionTab>> tabs;
  std::vector<std::unique_ptr<sessions::SessionTab>> candidates;
  for (auto it=content.windows.begin(); it!=content.windows.end(); ++it) {
    windows.push_back(std::move(it->second));
  }
  for (auto it=content.tabs.begin(); it!=content.tabs.end(); ++it) {
    if (std::find(ids.begin(), ids.end(), it->second->tab_id.id()) != ids.end()) {
      candidates.push_back(std::move(it->second));
    } else {
      tabs.push_back(std::move(it->second));
    }
  }

  SortTabs(tabs);
  SortTabs(candidates);

  StackController stack_controller;

  for (auto& candidate : candidates) {
    if (pinned.has_value()) {
      candidate->pinned = pinned.value();
    }
    if (group.has_value()) {
      stack_controller.track(candidate.get(), group.value());
      if (group.value().length()) {
        SetTabStackId(candidate.get(), group.value());
      } else {
        RemoveTabStackId(candidate.get());
      }
    }
    if (workspace.has_value()) {
      if (workspace.value() > 0) {
        SetWorkspaceId(candidate.get(), workspace.value());
      } else {
        RemoveWorkspaceId(candidate.get());
      }
    }
    if (window_id.has_value()) {
      // Moves to another window.
      candidate->window_id = SessionID::FromSerializedValue(window_id.value());
    }
  }

  int visual_index = 0;
  for (auto& tab : tabs) {
    stack_controller.append(tab.get());
    if (tab->tab_id.id() == before_tab_id) {
      for (auto& candidate : candidates) {
        candidate->tab_visual_index = visual_index++;
      }
    }
    tab->tab_visual_index = visual_index++;
  }
  if (before_tab_id < 0) {
    for (auto& candidate : candidates) {
      candidate->tab_visual_index = visual_index++;
    }
  }
  for (auto& candidate : candidates) {
    tabs.push_back(std::move(candidate));
  }

  SortTabs(tabs);

  stack_controller.purge(windows);

  // Build a command set.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ::vivaldi::VivaldiSessionService service(profile);
  bool has_commands = service.SetCommands(windows, tabs) > 0;
  if (has_commands) {
    if (!service.Save(path)) {
      return kErrorWriting;
    }
  }

  return kNoError;
}

int SetTabStack(content::BrowserContext* browser_context,
                base::FilePath path, std::vector<int32_t> ids,
                std::string group) {
  SessionContent content;
  GetContent(path, content);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  std::vector<std::unique_ptr<sessions::SessionTab>> tabs;
  std::vector<std::unique_ptr<sessions::SessionTab>> candidates;
  for (auto it=content.windows.begin(); it!=content.windows.end(); ++it) {
    windows.push_back(std::move(it->second));
  }
  for (auto it=content.tabs.begin(); it!=content.tabs.end(); ++it) {
    if (std::find(ids.begin(), ids.end(), it->second->tab_id.id()) != ids.end()) {
      candidates.push_back(std::move(it->second));
    } else {
      tabs.push_back(std::move(it->second));
    }
  }

  // Ensure that all modified tabs end up in the same stack
  if (!group.empty() && candidates.size() > 0) {
    bool pinned = candidates[0].get()->pinned;
    SessionID window_id = candidates[0].get()->window_id;
    std::optional<double> workspace_id = GetWorkspaceId(candidates[0].get());
    for (auto& candidate : candidates) {
      candidate.get()->pinned = pinned;
      candidate.get()->window_id = window_id;
      if (workspace_id.has_value()) {
        SetWorkspaceId(candidate.get(), workspace_id.value());
      } else {
        RemoveWorkspaceId(candidate.get());
      }
    }
  }

  StackController stack_controller;

  if (group.empty()) {
    for (auto& candidate : candidates) {
      stack_controller.track(candidate.get());
      RemoveTabStackId(candidate.get());
      tabs.push_back(std::move(candidate));
    }
  } else {
    for (auto& candidate : candidates) {
      stack_controller.track(candidate.get(), group);
      SetTabStackId(candidate.get(), group);
      tabs.push_back(std::move(candidate));
    }
  }
  for (auto& tab : tabs) {
    stack_controller.append(tab.get());
  }

  stack_controller.purge(windows);

  // Build a command set.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ::vivaldi::VivaldiSessionService service(profile);
  bool has_commands = service.SetCommands(windows, tabs) > 0;
  if (has_commands) {
    if (!service.Save(path)) {
      return kErrorWriting;
    }
  }

  return kNoError;
}

void SetTabStackForImportedTab(const base::Uuid id, sessions::SessionTab* tab) {
  // The tab has to have NO prior Json set....
  DCHECK(tab);

  // We want to keep what's in the tab ext data (in case we imported something
  // of value) just change the tab stack.
  if (tab->viv_ext_data.empty()) {
    // Write empty dict into ext data first.
    base::Value root(base::Value::Type::DICT);
    std::string serialized_json;
    base::JSONWriter::Write(base::ValueView(root), &serialized_json);
    tab->viv_ext_data = serialized_json;
  }

  SetTabStackId(tab, id.AsLowercaseString());
}

int SetWindow(content::BrowserContext* browser_context, base::FilePath path,
              std::vector<int32_t> ids,
              const std::vector<GroupAlias>& group_aliases) {
  SessionContent content;
  GetContent(path, content);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  std::vector<std::unique_ptr<sessions::SessionTab>> tabs;
  std::vector<std::unique_ptr<sessions::SessionTab>> candidates;

  for (auto it=content.windows.begin(); it!=content.windows.end(); ++it) {
    windows.push_back(std::move(it->second));
  }
  for (auto it=content.tabs.begin(); it!=content.tabs.end(); ++it) {
    if (std::find(ids.begin(), ids.end(), it->second->tab_id.id()) != ids.end()) {
      candidates.push_back(std::move(it->second));
    } else {
      tabs.push_back(std::move(it->second));
    }
  }

  if (candidates.size() == 0) {
    return kErrorUnknownId;
  }

  StackController stack_controller;

  std::unique_ptr<sessions::SessionWindow> window(new SessionWindow());
  window->window_id = SessionID::NewUnique();
  window->bounds = windows[0].get()->bounds;
  window->selected_tab_index = 0;
  window->type = SessionWindow::TYPE_NORMAL;
  window->is_constrained = false;
  window->timestamp = base::Time::Now();
  window->show_state = windows[0].get()->show_state;
  window->app_name = windows[0].get()->app_name;

  int visual_index = 0;
  for (auto& tab : tabs) {
    tab->tab_visual_index = visual_index++;
  }
  visual_index = 0;
  for (auto& candidate : candidates) {
    candidate->window_id = window->window_id;
    candidate->tab_visual_index = visual_index++;
    std::string group = GetTabStackId(candidate.get());
    if (!group.empty()) {
      stack_controller.track(candidate.get());
      std::string alias;
      for (auto& group_alias : group_aliases) {
        if (group_alias.group == group) {
          alias = group_alias.alias;
          break;
        }
      }
      if (alias.empty()) {
        return kErrorNoContent;
      }
      SetTabStackId(candidate.get(), alias);
      // It may be that only one element is moved to a new group. In that
      // case we will remove group info later on. But to do that we must add
      // information about the new group. So track and append.
      stack_controller.track(candidate.get());
      stack_controller.append(candidate.get());

    }
    // We are moving to a window so workspace information must be removed.
    RemoveWorkspaceId(candidate.get());
  }
  for (auto& tab : tabs) {
    stack_controller.append(tab.get());
  }

  for (auto& candidate : candidates) {
    tabs.push_back(std::move(candidate));
  }

  windows.push_back(std::move(window));

  stack_controller.purge(windows);

  // Build a command set.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ::vivaldi::VivaldiSessionService service(profile);
  bool has_commands = service.SetCommands(windows, tabs) > 0;
  if (has_commands) {
    if (!service.Save(path)) {
      return kErrorWriting;
    }
  }

  return kNoError;
}

int SetWorkspace(content::BrowserContext* browser_context, base::FilePath path,
                 std::vector<int32_t> ids,
                 double workspace_id,
                 const std::vector<GroupAlias>& group_aliases) {
  SessionContent content;
  GetContent(path, content);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  std::vector<std::unique_ptr<sessions::SessionTab>> tabs;
  std::vector<std::unique_ptr<sessions::SessionTab>> candidates;

  for (auto it=content.windows.begin(); it!=content.windows.end(); ++it) {
    windows.push_back(std::move(it->second));
  }
  for (auto it=content.tabs.begin(); it!=content.tabs.end(); ++it) {
    if (std::find(ids.begin(), ids.end(), it->second->tab_id.id()) != ids.end()) {
      candidates.push_back(std::move(it->second));
    } else {
      tabs.push_back(std::move(it->second));
    }
  }

  if (candidates.size() == 0) {
    return kErrorUnknownId;
  }

  StackController stack_controller;

  for (auto& candidate : candidates) {
    std::string group = GetTabStackId(candidate.get());
    if (!group.empty()) {
      stack_controller.track(candidate.get());
      std::string alias;
      for (auto& group_alias : group_aliases) {
        if (group_alias.group == group) {
          alias = group_alias.alias;
          break;
        }
      }
      if (alias.empty()) {
        return kErrorNoContent;
      }
      SetTabStackId(candidate.get(), alias);
      // It may be that only one element is moved to a new group. In that
      // case we will remove group info later on. But to do that we must add
      // information about the new group. So track and append.
      stack_controller.track(candidate.get());
      stack_controller.append(candidate.get());

    }
    SetWorkspaceId(candidate.get(), workspace_id);
  }
  for (auto& tab : tabs) {
    stack_controller.append(tab.get());
  }
  for (auto& candidate : candidates) {
    tabs.push_back(std::move(candidate));
  }

  stack_controller.purge(windows);

  // Build a command set.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ::vivaldi::VivaldiSessionService service(profile);
  bool has_commands = service.SetCommands(windows, tabs) > 0;
  if (has_commands) {
    if (!service.Save(path)) {
      return kErrorWriting;
    }
  }

  return kNoError;
}

int QuarantineTabs(BrowserContext* browser_context,
                   base::FilePath path,
                   bool value,
                   std::vector<int32_t> ids) {
  // Load content
  SessionContent content;
  GetContent(path, content);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  std::vector<std::unique_ptr<sessions::SessionTab>> tabs;
  for (auto it=content.windows.begin(); it!=content.windows.end(); ++it) {
    windows.push_back(std::move(it->second));
  }
  for (auto it = content.tabs.begin(); it != content.tabs.end(); ++it) {
    auto id_it = std::find(ids.begin(), ids.end(), it->second->tab_id.id());
    if (id_it != ids.end()) {
      SetTabFlag(it->second.get(), TAB_QUARANTNE, value);
      ids.erase(id_it);
    }
    tabs.push_back(std::move(it->second));
  }

  // Build a command set.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ::vivaldi::VivaldiSessionService service(profile);
  bool has_commands = service.SetCommands(windows, tabs) > 0;
  if (has_commands) {
    if (!service.Save(path)) {
      return kErrorWriting;
    }
  }

  return kNoError;
}

bool IsTabQuarantined(const SessionTab* tab) {
  std::optional<int> flag = GetTabFlag(tab);
  return flag.has_value() ? flag.value() & TAB_QUARANTNE : false;
}

std::string GetTabStackId(const SessionTab* tab) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> json =
     base::JSONReader::Read(tab->viv_ext_data, options);
  std::string value;
  if (json && json->is_dict()) {
    std::string* s = json->GetDict().FindString(kVivaldiTabStackId);
    if (s) {
      value = *s;
    }
  }
  return value;
}

bool GetFixedTabTitles(const sessions::SessionTab* tab,
                       std::string& title, std::string& groupTitle) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> json =
     base::JSONReader::Read(tab->viv_ext_data, options);
  if (json && json->is_dict()) {
    std::string* s = json->GetDict().FindString(kVivaldiFixedTitle);
    if (s) {
      title = *s;
    }
    s = json->GetDict().FindString(kVivaldiFixedGroupTitle);
    if (s) {
      groupTitle = *s;
    }
    return true;
  }
  return false;
}

int SetTabTitle(BrowserContext* browser_context,
                base::FilePath path,
                int32_t tab_id,
                std::string title) {
  // Load content
  SessionContent content;
  GetContent(path, content);

  SessionTab* tab = nullptr;
  for (auto tit = content.tabs.begin(); tit != content.tabs.end(); ++tit) {
    if (tit->second->tab_id.id() == tab_id) {
      tab = tit->second.get();
      break;
    }
  }
  if (!tab) {
    return kErrorNoContent;
  }

  // Save data to tab ext data.
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> json =
    base::JSONReader::Read(tab->viv_ext_data, options);
  if (json && json->is_dict()) {
    json->GetDict().Set(kVivaldiFixedTitle, title);
    base::JSONWriter::Write(base::ValueView(json.value()),
      &tab->viv_ext_data);
  }

  // Build a command set.
  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  std::vector<std::unique_ptr<sessions::SessionTab>> tabs;
  for (auto it=content.windows.begin(); it!=content.windows.end(); ++it) {
    windows.push_back(std::move(it->second));
  }
  for (auto it = content.tabs.begin(); it != content.tabs.end(); ++it) {
    tabs.push_back(std::move(it->second));
  }
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ::vivaldi::VivaldiSessionService service(profile);
  bool has_commands = service.SetCommands(windows, tabs) > 0;
  if (has_commands) {
    if (!service.Save(path)) {
      return kErrorWriting;
    }
  }

  return kNoError;
}

int SetTabStackTitle(BrowserContext* browser_context,
                     base::FilePath path,
                     std::vector<int32_t> tab_ids,
                     std::string title) {
  // Load content
  SessionContent content;
  GetContent(path, content);

  bool has_match = false;
  for (auto id: tab_ids) {
    for (auto tit = content.tabs.begin(); tit != content.tabs.end(); ++tit) {
      if (tit->second->tab_id.id() == id) {
        has_match = true;
        SessionTab* tab = tit->second.get();
        base::JSONParserOptions options = base::JSON_PARSE_RFC;
          std::optional<base::Value> json =
        base::JSONReader::Read(tab->viv_ext_data, options);
        if (json && json->is_dict()) {
          json->GetDict().Set(kVivaldiFixedGroupTitle, title);
          base::JSONWriter::Write(base::ValueView(json.value()),
            &tab->viv_ext_data);
        }
        break;
      }
    }
  }

  if (!has_match) {
    return kErrorNoContent;
  }

  // Build a command set.
  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  std::vector<std::unique_ptr<sessions::SessionTab>> tabs;
  for (auto it=content.windows.begin(); it!=content.windows.end(); ++it) {
    windows.push_back(std::move(it->second));
  }
  for (auto it = content.tabs.begin(); it != content.tabs.end(); ++it) {
    tabs.push_back(std::move(it->second));
  }
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ::vivaldi::VivaldiSessionService service(profile);
  bool has_commands = service.SetCommands(windows, tabs) > 0;
  if (has_commands) {
    if (!service.Save(path)) {
      return kErrorWriting;
    }
  }

  return kNoError;
}

std::unique_ptr<base::Value::Dict> GetTabStackTitles(
    const SessionWindow* window) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> json =
     base::JSONReader::Read(window->viv_ext_data, options);
  if (json && json->is_dict()) {
    base::Value::Dict* titles = json->GetIfDict()->FindDict(
        kVivaldiTabStackTitles);
    if (titles) {
      std::unique_ptr<base::Value::Dict> copy =
        std::make_unique<base::Value::Dict>(titles->Clone());
      return copy;
    }
  }
  return nullptr;
}

void GetContent(base::FilePath name, SessionContent& content) {
  ::vivaldi::VivaldiSessionService service;
  auto cmds = service.LoadSettingInfo(name);

  std::vector<std::unique_ptr<sessions::SessionCommand>> commands;
  for (auto &item: cmds) {
    // We don't build thumbnails here.
    if (item->id() == sessions::GetVivCreateThumbnailCommandId()) {
      continue;
    }
    commands.push_back(std::move(item));
  }

  sessions::TokenToSessionTabGroup tab_groups;
  sessions::VivaldiCreateTabsAndWindows(commands, &content.tabs,
                                        &tab_groups, &content.windows,
                                        &content.active_window_id);
}

void DumpContent(base::FilePath name) {
  /*
  SessionContent content;
  GetContent(name, content);
  for (auto it=content.tabs.begin(); it!=content.tabs.end(); ++it) {
    printf("tab: id: %d: winId: %d navIndex: %d numNavEntries: %ld\n",
      it->second->tab_id.id(),
      it->second->window_id.id(),
      it->second->current_navigation_index,
      it->second->navigations.size());
    const sessions::SerializedNavigationEntry& entry =
      it->second->navigations.at(it->second->current_navigation_index);
    printf("active nav entry: %d url: %s favicon: %s\n",
            entry.unique_id(),
            entry.virtual_url().spec().c_str(),
            entry.favicon_url().spec().c_str());
  }
  for (auto it=content.windows.begin(); it!=content.windows.end(); ++it) {
    printf("window: id: %d\n", it->second->window_id.id());
  }
  */
}

// Add index to list of tabs opened on the command-line, before tab to window
bool AddCommandLineTab(Browser* browser) {
  if (!browser)
    return false;
  std::string v_e_d = browser->viv_ext_data();
  std::optional<base::Value> json = std::nullopt;
  json = base::JSONReader::Read(v_e_d, base::JSON_PARSE_RFC);
  if (!(json && json->is_dict()))
    return false;
  auto* val = json->GetDict().EnsureList("commandline_tab");
  if (!val)
    return false;
  val->Append(browser->tab_strip_model()->count());
  std::string new_v_e_d;
  JSONStringValueSerializer serializer(&new_v_e_d);
  serializer.Serialize(*json);
  browser->set_viv_ext_data(new_v_e_d);
  return true;
}

bool IsVivaldiPanel(const sessions::tab_restore::Entry& entry) {
  if (entry.type != sessions::tab_restore::Type::TAB)
    return false;
  auto &tab = static_cast<const sessions::tab_restore::Tab&>(entry);
  return vivaldi::ParseVivPanelId(tab.viv_ext_data).has_value();
}

bool IsRestorableInVivaldi(const sessions::tab_restore::Entry& entry) {
  // Never restore panels or web-widgets.
  if (IsVivaldiPanel(entry))
    return false;

  // Restore, if the tab has a history.
  auto& tab = static_cast<const sessions::tab_restore::Tab&>(entry);
  if (tab.navigations.size() > 1)
    return true;

  // Makes no sense to restore a tab with no history.
  if (tab.navigations.empty())
    return false;

  // Don't restore new-tab-url in the history only.
  if (base::StartsWith(tab.navigations[0].original_request_url().spec(),
                 vivaldi::kVivaldiNewTabURL))
    return false;

  return true;
}
}  // namespace sessions
