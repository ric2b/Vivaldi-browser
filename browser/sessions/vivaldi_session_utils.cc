// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.

#include "browser/sessions/vivaldi_session_utils.h"

#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "browser/sessions/vivaldi_session_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/vivaldi_session_service_commands.h"
#include "extensions/api/sessions/vivaldi_sessions_api.h"
#include "sessions/index_codec.h"
#include "sessions/index_model.h"
#include "sessions/index_node.h"
#include "sessions/index_service_factory.h"
#include "sessions/index_storage.h"
#include "vivaldi/app/grit/vivaldi_native_strings.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"
#include "ui/base/l10n/l10n_util.h"

#include <algorithm>

using content::BrowserContext;
using sessions::Index_Model;
using sessions::Index_Node;
using sessions::IndexServiceFactory;

namespace {

const int kNumberBufferSize = 16;
// Note These must stay in sync with same symbols set in
// chromium/components/sessions/core/session_service_commands.cc
static const SessionCommand::id_type kCommandSetExtData = 21;
static const SessionCommand::id_type kCommandSetWindowExtData = 22;

const char kVivaldiTabFlag[] = "flag";
const char kVivaldiTabStackId[] = "group";
const char kVivaldiTabStackTitles[] = "groupTitles";

#define TAB_QUARANTNE 0x00

bool SetTabFlag(std::string& viv_extdata, int key, bool flag) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  absl::optional<base::Value> json =
     base::JSONReader::Read(viv_extdata, options);
  if (json && json->is_dict()) {
    absl::optional<int> candidate = json->FindDoubleKey(kVivaldiTabFlag);
    int value = candidate.has_value() ? candidate.value() : 0;
    if (flag) {
      value |= (1 >> key);
    } else {
      value ^= (1 >> key);
    }
    json->GetDict().Set(kVivaldiTabFlag, value);
    base::JSONWriter::Write(base::ValueView(json.value()), &viv_extdata);
    return true;
  }
  return false;
}

absl::optional<int> GetTabFlag(const std::string& viv_extdata) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  absl::optional<base::Value> json =
     base::JSONReader::Read(viv_extdata, options);
  absl::optional<int> value;
  if (json && json->is_dict()) {
    value = json->FindIntKey(kVivaldiTabFlag);
  }
  return value;
}

// Returns true when 'ids' refers to one or more tabs in the browser.
bool ContainsTabs(Browser* browser, std::vector<int>& ids) {
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
  return base::FilePath();
}

int CopySessionFile(BrowserContext* browser_context,
                    sessions::WriteSessionOptions& opts) {

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

    error_code = SetNodeStateFromPath(browser_context, ctl.path, node.get());
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
    // Test for number of children and remove oldest if needed.
    Profile* profile = Profile::FromBrowserContext(browser_context);
    size_t size = autosave_node->children().size();
    size_t max = profile->GetPrefs()->GetInteger(
        vivaldiprefs::kSessionsExitSaveListSize);
    if (size >= max) {
      Index_Node* child = autosave_node->children()[size - 1].get();
      error_code = DeleteSessionFile(browser_context, child);
      // Allow a missing session file when we are deleting.
      if (error_code == sessions::kErrorFileMissing) {
        error_code = sessions::kNoError;
      }
      if (error_code == sessions::kNoError) {
        model->Remove(child);
      }
    }

    // New child of the node we are about to update. Holds old state of node.
    std::unique_ptr<Index_Node> child = std::make_unique<Index_Node>(
        base::GenerateGUID(), Index_Node::GetNewId());
    child->Copy(autosave_node);
    child->SetContainerGuid(autosave_node->guid());

    // Placeholder for transferring updated data to existing node.
    std::unique_ptr<Index_Node> tmp = std::make_unique<Index_Node>("", -1);
    tmp->SetFilename(ctl.filename); // Must be set if calling DeleteSessionFile

    error_code = SetNodeStateFromPath(browser_context, ctl.path, tmp.get());
    if (error_code != sessions::kNoError) {
      DeleteSessionFile(browser_context, tmp.get());
      return error_code;
    }
    // Entries we do not want to modify when updating below.
    tmp->SetTitle(autosave_node->GetTitle());
    tmp->SetCreateTime(autosave_node->create_time());
    // The typical case for overriding the modify time is when we update the
    // auto save list with a backup file while loading the model. The code above
    // (WriteSessionFile) will then do a file copy operation casing the modify
    // time to be 'now'. That time should be when the backup got created.
    if (modify_time) {
      tmp->SetModifyTime(*modify_time);
    }

    // Update the existing node.
    model->Change(autosave_node, tmp.get());
    // Add child to the node we have updated.
    model->Add(std::move(child), autosave_node, 0, "");
  }

  // Always delete any backup when auto save list is updated.
  if (model->backup_node()) {
    DeleteSessionFile(browser_context, model->backup_node());
    model->Remove(model->backup_node());
  }

  return error_code;
}


}  // namespace

namespace sessions {

WriteSessionOptions::WriteSessionOptions() {}
WriteSessionOptions::~WriteSessionOptions() {}

SessionContent::SessionContent() {}
SessionContent::~SessionContent() {}

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

int SetNodeStateFromPath(BrowserContext* browser_context,
                         const base::FilePath& path, Index_Node* node) {
  // Get time from file. Same as when setting up the model on first run.
  base::File::Info info;
  if (!base::GetFileInfo(path, &info)) {
    return kErrorMissingName;
  }

  base::Time created = info.creation_time;
  node->SetCreateTime(created.ToJsTime());
  node->SetModifyTime(created.ToJsTime());

  // Get data from the file.
  SessionContent content;
  GetContent(path, content);

  int num_windows = content.windows.size();
  int num_tabs = content.tabs.size();
  // It is an error if there are no tabs or no windows.
  if (num_windows == 0 || num_tabs == 0) {
    return kErrorNoContent;
  }
  node->SetWindowsCount(num_windows);
  node->SetTabsCount(num_tabs);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  bool save_workspaces =
    profile->GetPrefs()->GetBoolean(
        vivaldiprefs::kSessionsSaveAllWorkspaces);
  if (save_workspaces) {
    const auto& list = profile->GetPrefs()->GetList(
        vivaldiprefs::kWorkspacesList);
    base::Value::List workspaces(list.Clone());
    node->SetWorkspaces(std::move(workspaces));
  }
  return kNoError;
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
  for (auto* browser : *BrowserList::GetInstance()) {
    // 1 The tracking test will prevent saving content in private (or guest)
    //   windows from regular (but private can save regular).
    // 2 Make sure the browser has tabs and a window. Browser's destructor
    //   removes itself from the BrowserList. When a browser is closed the
    //   destructor is not necessarily run immediately. This means it's
    //   possible for us to get a handle to a browser that is about to be
    //   removed. If the tab count is 0 or the window is NULL, the browser
    //   is about to be deleted, so we ignore it.
    if (service.ShouldTrackWindow(browser) &&
        browser->tab_strip_model()->count() &&
        browser->window() &&
        // An empty list means all tabs.
        (opts.ids.size() == 0 || ContainsTabs(browser, opts.ids)) &&
        // A window id of 0 means all windows.
        (opts.window_id == 0 || (browser->session_id().id() == opts.window_id))) {
      service.BuildCommandsForBrowser(browser, opts.ids);
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

// Moves a backup session to the auto saved session list. Intended to be used
// while loading the model.
int AutoSaveFromBackup(BrowserContext* browser_context) {
  Index_Model* model = IndexServiceFactory::GetForBrowserContext(
      browser_context);
  if (!model || !model->items_node() || !model->backup_node()) {
    return sessions::kErrorNoModel;
  }

  // If auto saving is turned off backup genearation is also turned off. Should
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

void AutoSave(BrowserContext* browser_context) {
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

bool SetQuarantine(BrowserContext* browser_context,
                   base::FilePath name,
                   bool value,
                   std::vector<int32_t> ids) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ::vivaldi::VivaldiSessionService service(profile);

  auto cmds = service.LoadSettingInfo(name);
  std::vector<std::unique_ptr<sessions::SessionCommand>> commands;
  commands.swap(cmds);

  bool changed = false;
  for (std::unique_ptr<sessions::SessionCommand>& cmd : commands) {
    bool match = false;
    if (cmd->id() == kCommandSetExtData && !ids.empty()) {
      SessionID id = SessionID::InvalidValue();
      std::string ext_data;
      vivaldi::RestoreSetVivExtDataCommand(*cmd, &id, &ext_data);

      std::vector<int32_t>::iterator it = std::find(ids.begin(), ids.end(),
                                                    id.id());
      if (it != ids.end()) {
        match = true;
        changed = true;
        SetTabFlag(ext_data, TAB_QUARANTNE, value);
        service.ScheduleCommand(
            vivaldi::CreateSetTabVivExtDataCommand(cmd->id(), id, ext_data));
        ids.erase(it);
      }
    }
    if (!match) {
      service.ScheduleCommand(std::move(cmd));
    }
  }

  service.Save(name);

  return changed;
}

bool IsQuarantined(const std::string& ext_data) {
  absl::optional<int> flag = GetTabFlag(ext_data);
  return flag.has_value() ? flag.value() & (1 >> TAB_QUARANTNE) : false;
}

std::string GetTabStackId(const std::string& ext_data) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  absl::optional<base::Value> json =
     base::JSONReader::Read(ext_data, options);
  std::string value;
  if (json && json->is_dict()) {
    std::string* s = json->FindStringKey(kVivaldiTabStackId);
    if (s) {
      value = *s;
    }
  }
  return value;
}

bool SetTabStackTitle(BrowserContext* browser_context,
                      base::FilePath name,
                      int32_t window_id,
                      std::string group,
                      std::string title) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ::vivaldi::VivaldiSessionService service(profile);

  auto cmds = service.LoadSettingInfo(name);
  std::vector<std::unique_ptr<sessions::SessionCommand>> commands;
  commands.swap(cmds);

  bool changed = false;
  for (std::unique_ptr<sessions::SessionCommand>& cmd : commands) {
    bool match = false;
    if (cmd->id() == kCommandSetWindowExtData) {
      std::string ext_data;
      SessionID id = SessionID::InvalidValue();
      vivaldi:: RestoreSetWindowVivExtDataCommand(*cmd, &id, &ext_data);
      if (id.id() == window_id) {
        // Parse ext_data into json, update json, write to ext_data and save to
        // command again.
        base::JSONParserOptions options = base::JSON_PARSE_RFC;
        absl::optional<base::Value> json =
          base::JSONReader::Read(ext_data, options);
        if (json && json->is_dict()) {
          match = true;
          changed = true;
          base::Value::Dict* titles = json->GetIfDict()->FindDict(
              kVivaldiTabStackTitles);
          if (titles) {
            titles->Set(group, title);
            json->GetIfDict()->Set(kVivaldiTabStackTitles, std::move(*titles));
          } else {
            base::Value::Dict dict;
            dict.Set(group, title);
            json->GetIfDict()->Set(kVivaldiTabStackTitles, std::move(dict));
          }
          base::JSONWriter::Write(base::ValueView(json.value()), &ext_data);
           service.ScheduleCommand(
              vivaldi::CreateSetTabVivExtDataCommand(cmd->id(), id, ext_data));
        }
      }
    }
    if (!match) {
      service.ScheduleCommand(std::move(cmd));
    }
  }
  service.Save(name);

  return changed;
}

// ext_data must be fecthed from the window object (not tab)
std::unique_ptr<base::Value::Dict> GetTabStackTitles(
    const std::string& ext_data) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  absl::optional<base::Value> json =
     base::JSONReader::Read(ext_data, options);
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
  commands.swap(cmds);
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

}  // namespace sessions
