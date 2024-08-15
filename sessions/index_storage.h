// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SESSIONS_INDEX_STORAGE_H_
#define SESSIONS_INDEX_STORAGE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/memory/ref_counted.h"

namespace base {
class SequencedTaskRunner;
class Value;
}  // namespace base

namespace content {
class BrowserContext;
}

namespace sessions {

class Index_Model;
class Index_Node;

class IndexLoadDetails {
 public:
  explicit IndexLoadDetails(Index_Node* items_node, Index_Node* backup_node,
                            Index_Node* persistent_node);
  ~IndexLoadDetails();
  IndexLoadDetails(const IndexLoadDetails&) = delete;
  IndexLoadDetails& operator=(const IndexLoadDetails&) = delete;

  void SetLoadedFromFileScan(bool loaded_from_filescan)
    {loaded_from_filescan_ = loaded_from_filescan; }
  void SetLoadingFailed(bool loading_failed)
    {loading_failed_ = loading_failed; }
  bool get_loaded_from_filescan() const { return loaded_from_filescan_; }
  bool get_loading_failed() const { return loading_failed_; }

  std::unique_ptr<Index_Node> release_items_node() {
    return std::move(items_node_);
  }

  std::unique_ptr<Index_Node> release_backup_node() {
    return std::move(backup_node_);
  }

  std::unique_ptr<Index_Node> release_persistent_node() {
    return std::move(persistent_node_);
  }

  Index_Node* items_node() const { return items_node_.get(); }
  Index_Node* backup_node() const { return backup_node_.get(); }
  Index_Node* persistent_node() const { return persistent_node_.get(); }

 private:
  std::unique_ptr<Index_Node> items_node_;
  std::unique_ptr<Index_Node> backup_node_;
  std::unique_ptr<Index_Node> persistent_node_;
  bool loaded_from_filescan_ = false;
  bool loading_failed_ = false;
};

class IndexStorage : public base::ImportantFileWriter::DataSerializer {
 public:
  IndexStorage(content::BrowserContext* context,
               Index_Model* model,
               base::SequencedTaskRunner* sequenced_task_runner);

  ~IndexStorage() override;
  IndexStorage(const IndexStorage&) = delete;
  IndexStorage& operator=(const IndexStorage&) = delete;

  static const base::FilePath::CharType* GetFolderName();

  // Loads data into the model, notifying the model when done. This
  // takes ownership of |details|.
  void Load(std::unique_ptr<IndexLoadDetails> details);

  // Schedules saving data to disk.
  void ScheduleSave();

  void OnModelWillBeDeleted();

  // Callback from backend after loading the file
  void OnLoadFinished(std::unique_ptr<IndexLoadDetails> details);

  // ImportantFileWriter::DataSerializer implementation.
  std::optional<std::string> SerializeData() override;

  bool SaveValue(const std::unique_ptr<base::Value>& value);

 private:
  // Backup is done once and only if a regular save is attempted.
  enum BackupState {
    BACKUP_NONE,        // No backup attempted
    BACKUP_DISPATCHED,  // Request posted
    BACKUP_ATTEMPTED    // Backup has been called.
  };

  friend base::RefCountedThreadSafe<IndexStorage>;

  void OnBackupFinished();

  // Serializes the data and schedules save using ImportantFileWriter.
  // Returns true on successful serialization.
  bool SaveNow();

  raw_ptr<Index_Model> model_;

  base::FilePath directory_;

  // Path to the file where we can read and write data (in profile).
  base::ImportantFileWriter writer_;

  // Sequenced task runner where file I/O operations will be performed at.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  BackupState backup_state_ = BACKUP_NONE;

  base::WeakPtrFactory<IndexStorage> weak_factory_;
};

}  // namespace sessions

#endif  // SESSIONS_INDEX_STORAGE_H_
