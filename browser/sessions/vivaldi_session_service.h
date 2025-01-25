//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#ifndef BROWSER_SESSIONS_VIVALDI_SESSION_SERVICE_H_
#define BROWSER_SESSIONS_VIVALDI_SESSION_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_delegate.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "components/datasource/vivaldi_image_store.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_constants.h"
#include "components/sessions/core/session_service_commands.h"


namespace vivaldi {

struct SessionOptions {
  SessionOptions();
  ~SessionOptions();

  // Always open a new window and start loading tabs from there
  bool newWindow_ = false;
  // Load all tabs into the same window
  bool oneWindow_ = false;
  // Include tabs that are part of a workspace
  bool withWorkspace_ = true;
  // Only open tabs (given by id) in this list. All if list is empty
  std::vector<int32_t> tabs_to_include_;
};

// TODO: Rename this from Service to helper.
// We cannot inherit from SessionService, and changing that class to be
// suitable is risky and requires significant changes.
class VivaldiSessionService {
 public:
  typedef std::map<SessionID::id_type, std::pair<int, int>> IdToRange;
  typedef sessions::SessionCommand::id_type id_type;
  typedef sessions::SessionCommand::size_type size_type;

  explicit VivaldiSessionService(Profile* profile);
  VivaldiSessionService();
  ~VivaldiSessionService();
  VivaldiSessionService(const VivaldiSessionService&) = delete;
  VivaldiSessionService& operator=(const VivaldiSessionService&) = delete;

  bool ShouldTrackWindow(Browser* browser);
  void ScheduleCommand(std::unique_ptr<sessions::SessionCommand> command);

  void BuildCommandsForBrowser(Browser* browser,
      const std::vector<int>& ids,
      const vivaldi_image_store::Batch &batch);

  bool Save(const base::FilePath& file_name);
  int  Load(const base::FilePath& file_name,
            Browser* browser,
            const SessionOptions& opts);
  std::vector<std::unique_ptr<sessions::SessionCommand>> LoadSettingInfo(
      const base::FilePath& path);

  int SetCommands(
    std::vector<std::unique_ptr<sessions::SessionWindow>>& windows,
    std::vector<std::unique_ptr<sessions::SessionTab>>& tabs);

  std::vector<std::string> CollectThumbnailUrls(Browser* browser, const std::vector<int>& ids);

 private:
  void BuildCommandsForTab(const SessionID& window_id,
                           content::WebContents* tab,
                           int index_in_window,
                           bool is_pinned);

  void BuildCommandsForTabs(Browser* browser, const std::vector<int>& ids);
  void ResetFile(const base::FilePath& file_name);
  base::File* OpenAndWriteHeader(const base::FilePath& path);
  bool AppendCommandsToFile(
      base::File* file,
      const std::vector<std::unique_ptr<sessions::SessionCommand>>& commands);
  bool Read(base::File* file,
            std::vector<std::unique_ptr<sessions::SessionCommand>>* commands);
  bool FillBuffer(base::File* file);
  Browser* ProcessSessionWindows(
      std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
      const SessionID& active_window_id,
      std::vector<SessionRestoreDelegate::RestoredTab>* created_contents);
  void RestoreTabsToBrowser(
      const sessions::SessionWindow& window,
      Browser* browser,
      int initial_tab_count,
      int selected_tab_index,
      std::vector<SessionRestoreDelegate::RestoredTab>* created_contents);
  bool HasTabs(const sessions::SessionWindow& window);
  void RemoveUnusedRestoreWindows(
      std::vector<std::unique_ptr<sessions::SessionWindow>>* window_list);
  Browser* CreateRestoredBrowser(Browser::Type type,
                                 gfx::Rect bounds,
                                 ui::mojom::WindowShowState show_state,
                                 const std::string& app_name);
  content::WebContents* RestoreTab(const sessions::SessionTab& tab,
                                   const int tab_index,
                                   Browser* browser,
                                   bool is_selected_tab);
  void NotifySessionServiceOfRestoredTabs(Browser* browser, int initial_count);
  void ShowBrowser(Browser* browser, int selected_tab_index);

  void SetCommandsForWindow(
    const sessions::SessionWindow& window,
    std::vector<std::unique_ptr<sessions::SessionTab>>& tabs);
  void SetCommandsForTab(const sessions::SessionTab& tab);

  // Reads a single command, returning it. A return value of NULL indicates
  // either there are no commands, or there was an error. Use errored_ to
  // distinguish the two. If NULL is returned, and there is no error, it means
  // the end of file was successfully reached.
  std::unique_ptr<sessions::SessionCommand> ReadCommand(base::File* file);

  const std::vector<std::unique_ptr<sessions::SessionCommand>>&
  pending_commands() {
    return pending_commands_;
  }
  // Commands we need to send over to the backend.
  std::vector<std::unique_ptr<sessions::SessionCommand>> pending_commands_;

  // Maps from session tab id to the range of navigation entries that has
  // been written to disk.
  //
  // This is only used if not all the navigation entries have been
  // written.
  IdToRange tab_to_available_range_;

  // Handle to the target file.
  std::unique_ptr<base::File> current_session_file_;

  // Whether an error condition has been detected
  bool errored_;

  // As we read from the file, data goes here.
  std::string buffer_;

  // Position in buffer_ of the data.
  size_t buffer_position_;

  // Number of available bytes; relative to buffer_position_.
  size_t available_count_;

  // When loading a session, restore the first windows' tabs to this browser
  raw_ptr<Browser> browser_ = nullptr;

  const raw_ptr<Profile> profile_;

  // Session open options
  SessionOptions opts_;
};

}  // namespace vivaldi

#endif  // BROWSER_SESSIONS_VIVALDI_SESSION_SERVICE_H_
