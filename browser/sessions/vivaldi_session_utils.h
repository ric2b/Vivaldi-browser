// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.
#pragma once
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/values.h"
#include "browser/sessions/vivaldi_session_service.h"
#include "components/datasource/vivaldi_image_store.h"
#include "components/sessions/core/tab_restore_types.h"
#include "components/sessions/vivaldi_session_service_commands.h"

class Browser;

namespace content {
class BrowserContext;
}

namespace sessions {

class Index_Node;

enum SessionErrorCodes {
  kNoError,
  kErrorMissingName,
  kErrorWriting,
  kErrorFileMissing,
  kErrorDeleteFailure,
  kErrorFileExists,
  kErrorFileMoveFailure,
  kErrorLoadFailure,
  kErrorFileCopyFailure,
  kErrorUnknownId,
  kErrorNoModel,
  kErrorNoContent,
  kErrorWrongProfile,
  kErrorEmpty,
};

struct SessionContent {
  SessionContent();
  ~SessionContent();
  IdToSessionTab tabs;
  TokenToSessionTabGroup tab_groups;
  IdToSessionWindow windows;
  SessionID active_window_id = SessionID::InvalidValue();
};

struct WriteSessionOptions {
  WriteSessionOptions();
  ~WriteSessionOptions();
  // Input
  int from_id = -1;
  int window_id = 0;
  std::vector<int> ids;
  // Input + output
  std::string filename;
  // Output
  base::FilePath path;

  // thumbnails to be written
  vivaldi_image_store::Batch thumbnails;
};

struct GroupAlias {
  std::string group;
  std::string alias;
};

// Opens a session.
int Open(Browser* browser, Index_Node* node,
         const ::vivaldi::SessionOptions& opts);
// Opens a session with persistent tabs or just removes them if discard is true.
int OpenPersistentTabs(Browser* browser, bool discard);
// Returns the full path to the session file.
base::FilePath GetPathFromNode(content::BrowserContext* browser_context,
                               Index_Node* node);
// Updates node settings with data from session file.

int SetNodeState(content::BrowserContext* browser_context,
                 const base::FilePath& file, bool is_new, Index_Node* node);
// Writes session file with content and location controlled by opts.
int WriteSessionFile(content::BrowserContext* browser_context,
                     WriteSessionOptions& opts);

// Returns list of the thumbnail referenced by opts.ids.
std::vector<std::string>
CollectThumbnailUrls(content::BrowserContext* browser_context,
    const WriteSessionOptions& opts);

// Returns list of all the thumbnail urls.
std::vector<std::string>
CollectAllThumbnailUrls(content::BrowserContext* browser_context);

// Removes session file.
int DeleteSessionFile(content::BrowserContext* browser_context,
                      Index_Node* node);
int MoveAutoSaveNodesToTrash(content::BrowserContext* browser_context);
// Returns nodes that are too old to be kept in the auto save list. The
// cut off time depends on 'on_add'. If set the modified_time of the autosave
// session folder node is used, otherwise base::Time::Now(). "days before" will
// in either cases be subtraced from the computed time.
int GetExpiredAutoSaveNodes(content::BrowserContext* browser_context,
                            int days_before,
                            bool on_add,
                            std::vector<Index_Node*>& nodes);
// Saves current set of tabs to a fixed auto save node.
void AutoSave(content::BrowserContext* browser_context, bool from_ui = false);
// Moves timed backup into auto saved sessions.
int AutoSaveFromBackup(content::BrowserContext* browser_context);
// Saves tabs to a special session that holds persistent entries.
int SavePersistentTabs(content::BrowserContext* browser_context,
                     std::vector<int> ids);
// Remove given tabs from session file.
int DeleteTabs(content::BrowserContext* browser_context,
               base::FilePath path,
               std::vector<int32_t> ids);
// Pin or unpin tabs.
int PinTabs(content::BrowserContext* browser_context,
            base::FilePath path, bool value, std::vector<int32_t> ids);
// Move one or more tabs to window and index.
int MoveTabs(content::BrowserContext* browser_context,
             base::FilePath path,
             std::vector<int32_t> ids,
             int before_tab_id,
             std::optional<int32_t> window_id,
             std::optional<bool> pinned,
             std::optional<std::string> group,
             std::optional<double> workspace);
// Turn tabs specified in 'ids' into tabstack tabs or removes the tabs if
// group is empty.
int SetTabStack(content::BrowserContext* browser_context,
                base::FilePath path,
                std::vector<int32_t> ids,
                std::string group);

/// For an imported tab with NO viv_ext_data set, we set a tab stack id.
void SetTabStackForImportedTab(const base::Uuid id, sessions::SessionTab *tab);

// Moves specified tabs into a new window. Pinned state and tab stacks are kept
// (the latter if at least two tabs from a stack is affected). Workspace
// information is removed.
int SetWindow(content::BrowserContext* browser_context, base::FilePath path,
              std::vector<int32_t> ids,
              const std::vector<GroupAlias>& group_aliases);
// Moves specified tabs into a new workspace. Pinned state and tab stacks are
// kept (the latter if at least two tabs from a stack is affected).
int SetWorkspace(content::BrowserContext* browser_context, base::FilePath path,
                 std::vector<int32_t> ids,
                 double workspace_id,
                 const std::vector<GroupAlias>& group_aliases);
// Sets quarantine state for the list of tabs given by the ids.
int QuarantineTabs(content::BrowserContext* browser_context,
                   base::FilePath name, bool value, std::vector<int32_t> ids);
// Returns quarantine state.
bool IsTabQuarantined(const SessionTab* tab);
// Saves title. tab_id must refer to tabs that are regular tabs.
int SetTabTitle(content::BrowserContext* browser_context,
                base::FilePath path, int32_t tab_id, std::string title);
// Saves title. tab_ids must refer to tabs that are part of a tab stack.
int SetTabStackTitle(content::BrowserContext* browser_context,
                     base::FilePath path,
                     std::vector<int32_t> tab_ids,
                     std::string title);
// Returns tab stack id of a tab. String is empty if no stack id is set.
std::string GetTabStackId(const SessionTab* tab);
// Returns all tabstacks of a window (id and optionally title).
// ext data must be fetched from the window object (not tab). By VB-23686 this
// data is not written to the window segment anymore.
std::unique_ptr<base::Value::Dict> GetTabStackTitles(
  const SessionWindow* window);
// Looks up tab and stack titles of a tab and, if set, assigns to 'title'
// and 'groupTitle'. These are titles added by user, not the title of the active
// page.
bool GetFixedTabTitles(const sessions::SessionTab* tab,
                       std::string& title, std::string& groupTitle);

void GetContent(base::FilePath name, SessionContent& content);
// Dump cpntent of a tab
void DumpContent(base::FilePath name);

// Add index to list of tabs opened on the command-line, before tab to window
bool AddCommandLineTab(Browser* browser);

// Returns true if the entry is a web-widget or a panel.
bool IsVivaldiPanel(const sessions::tab_restore::Entry& entry);

// Returns true if we can restore the tab.
bool IsRestorableInVivaldi(const sessions::tab_restore::Entry& entry);
}  // namespace sessions
