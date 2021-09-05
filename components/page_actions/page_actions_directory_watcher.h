// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_DIRECTORY_WATCHER_H_
#define COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_DIRECTORY_WATCHER_H_

#include <string>
#include "base/callback.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"

namespace page_actions {
class DirectoryWatcher {
 public:
  struct Deleter {
    void operator()(const DirectoryWatcher* ptr);
  };

  using UpdatedFileContents =
      std::map<base::FilePath, std::map<base::FilePath, std::string>>;

  using ChangesCallback =
      base::RepeatingCallback<void(UpdatedFileContents,
                                   std::vector<base::FilePath>)>;

  explicit DirectoryWatcher(ChangesCallback callback,
                            base::FilePath apk_assets);

  void AddPaths(const std::vector<base::FilePath>& paths);
  void RemovePath(const base::FilePath& path);

 private:
  void Destroy() const { delete this; }
  using FilePathTimesMap = std::unordered_map<base::FilePath, base::Time>;

  void DoAddPaths(const std::vector<base::FilePath>& path);
  void DoRemovePath(const base::FilePath& path);
  void OnPathChanged(const base::FilePath& path, bool error);

  void ReportChanges();
  void end_update_cooldown() { in_update_cooldown_ = false; }

  FilePathTimesMap GetModificationTimes(const base::FilePath& path);

  ~DirectoryWatcher();
  DirectoryWatcher(const DirectoryWatcher&) = delete;
  DirectoryWatcher& operator=(const DirectoryWatcher&) = delete;

  const ChangesCallback callback_;
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;
  std::map<base::FilePath, std::unique_ptr<base::FilePathWatcher>>
      path_watchers_;
  std::map<base::FilePath, FilePathTimesMap> file_path_times_;

  base::FilePath apk_assets_;
  std::set<base::FilePath> pending_paths_;
  bool in_update_cooldown_ = false;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace page_actions

#endif  // COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_DIRECTORY_WATCHER_H_
