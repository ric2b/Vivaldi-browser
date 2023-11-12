// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/values.h"
#include "browser/sessions/vivaldi_session_service.h"
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
};

struct GroupAlias {
  std::string group;
  std::string alias;
};

// Opens a session.
int Open(Browser* browser, Index_Node* node,
         const ::vivaldi::SessionOptions& opts);
// Opens a session with persistent tabs.
int OpenPersistentTabs(Browser* browser);
// Returns the full path to the session file.
base::FilePath GetPathFromNode(content::BrowserContext* browser_context,
                               Index_Node* node);
// Updates node settings with data from session file.

int SetNodeState(content::BrowserContext* browser_context,
                 const base::FilePath& file, bool is_new, Index_Node* node);
// Writes session file with content and location controlled by opts.
int WriteSessionFile(content::BrowserContext* browser_context,
                     WriteSessionOptions& opts);
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
void AutoSave(content::BrowserContext* browser_context);
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
            base::FilePath path, std::vector<int32_t> ids, int before_tab_id,
            absl::optional<int32_t> window_id, absl::optional<bool> pinned,
            absl::optional<std::string> group, absl::optional<double> workspace);
// Turn tabs specified in 'ids' into tabstack tabs or removes the tabs if
// group is empty.
int SetTabStack(content::BrowserContext* browser_context, base::FilePath path,
                 std::vector<int32_t> ids, std::string group);
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
// Saves title. tab_id must refer to a tab that is part of a tab stack.
int SetTabStackTitle(content::BrowserContext* browser_context,
                     base::FilePath name, int32_t tab_id, std::string title);
// Returns tab stack id of a tab. String is empty if no stack id is set.
std::string GetTabStackId(const SessionTab* tab);
// Returns all tabstacks of a window (id and opitonally title).
// ext data must be fecthed from the window object (not tab).
std::unique_ptr<base::Value::Dict> GetTabStackTitles(
  const SessionWindow* window);

void GetContent(base::FilePath name, SessionContent& content);
// Dump cpntent of a tab
void DumpContent(base::FilePath name);

// Add index to list of tabs opened on the command-line, before tab to window
bool AddCommandLineTab(Browser* browser);

}  // namespace sessions
