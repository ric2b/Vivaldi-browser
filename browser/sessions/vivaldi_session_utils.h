// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/values.h"
#include "components/sessions/vivaldi_session_service_commands.h"

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

// Returns the full path to the session file.
base::FilePath GetPathFromNode(content::BrowserContext* browser_context,
                               Index_Node* node);
// Updates node settings with data from session file.
int SetNodeStateFromPath(content::BrowserContext* browser_context,
                         const base::FilePath& path, Index_Node* node);
// Writes session file with content and location controlled by opts.
int WriteSessionFile(content::BrowserContext* browser_context,
                     WriteSessionOptions& opts);
// Removes session file.
int DeleteSessionFile(content::BrowserContext* browser_context,
                      Index_Node* node);
// Saves current set of tabs to a fixed auto save node.
void AutoSave(content::BrowserContext* browser_context);
// Moves timed backup into auto saved sessions.
int AutoSaveFromBackup(content::BrowserContext* browser_context);
// Sets quarantine flag in ext data of given tabs.
bool SetQuarantine(content::BrowserContext* browser_context,
                   base::FilePath name, bool value, std::vector<int32_t> ids);
// Returns true if ext data holds a quarantine flag set to true.
bool IsQuarantined(const std::string& viv_extdata);

bool SetTabStackTitle(content::BrowserContext* browser_context,
                      base::FilePath name,
                      int32_t window_id,
                      std::string group,
                      std::string title);
// Returns tab stack id of a tab. Returned string is empty if no stack id is set.
std::string GetTabStackId(const std::string& viv_extdata);
// Returns all tabstacks of a window (id and opitonally title).
// ext data must be fecthed from the window object (not tab).
std::unique_ptr<base::Value::Dict> GetTabStackTitles(
    const std::string& viv_extdata);
void GetContent(base::FilePath name, SessionContent& content);
// Dump cpntent of a tab
void DumpContent(base::FilePath name);

}  // namespace sessions