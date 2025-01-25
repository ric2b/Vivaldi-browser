// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MENUS_MENU_STORAGE_H_
#define MENUS_MENU_STORAGE_H_

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

namespace menus {

struct Menu_Control;
class Menu_Model;
class Menu_Node;

class MenuLoadDetails {
 public:
  enum Mode { kLoad, kResetById, kResetByName, kResetAll };
  explicit MenuLoadDetails(Menu_Node* mainmenu,
                           Menu_Control* control,
                           int64_t id,
                           bool force_bundle,
                           Mode mode);
  explicit MenuLoadDetails(Menu_Node* mainmenu,
                           Menu_Control* control,
                           const std::string& menu,
                           bool force_bundle,
                           Mode mode);
  ~MenuLoadDetails();
  MenuLoadDetails(const MenuLoadDetails&) = delete;
  MenuLoadDetails& operator=(const MenuLoadDetails&) = delete;

  void SetUpgradeRoot(std::unique_ptr<base::Value> upgrade_root);

  Menu_Node* mainmenu_node() const { return mainmenu_node_.get(); }
  Menu_Control* control() const { return control_.get(); }

  int64_t id() { return id_; }
  const std::string& menu() { return menu_; }
  bool has_upgraded() const { return upgrade_root_.get() != nullptr; }
  bool force_bundle() const { return force_bundle_; }
  Mode mode() const { return mode_; }
  std::unique_ptr<Menu_Node> release_mainmenu_node() {
    return std::move(mainmenu_node_);
  }
  std::unique_ptr<Menu_Control> release_control() {
    return std::move(control_);
  }
  const std::unique_ptr<base::Value>& get_upgrade_root() {
    return upgrade_root_;
  }

 private:
  std::unique_ptr<Menu_Node> mainmenu_node_;
  std::unique_ptr<Menu_Control> control_;
  std::unique_ptr<base::Value> upgrade_root_;
  int64_t id_;
  bool force_bundle_;
  Mode mode_;
  std::string menu_;
};

class MenuStorage : public base::ImportantFileWriter::DataSerializer {
 public:
  MenuStorage(content::BrowserContext* context,
              Menu_Model* model,
              base::SequencedTaskRunner* sequenced_task_runner);

  ~MenuStorage() override;
  MenuStorage(const MenuStorage&) = delete;
  MenuStorage& operator=(const MenuStorage&) = delete;

  // Loads the notes into the model, notifying the model when done. This
  // takes ownership of |details|. See NotesLoadDetails for details.
  void Load(std::unique_ptr<MenuLoadDetails> details);

  // Schedules saving the notes model to disk.
  void ScheduleSave();

  // Notification the notes model is going to be deleted. If there is
  // a pending save, it is saved immediately.
  void OnModelWillBeDeleted();

  // Callback from backend after loading the file
  void OnLoadFinished(std::unique_ptr<MenuLoadDetails> details);

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

  friend base::RefCountedThreadSafe<MenuStorage>;

  void OnBackupFinished();

  // Serializes the data and schedules save using ImportantFileWriter.
  // Returns true on successful serialization.
  bool SaveNow();

  raw_ptr<Menu_Model> model_;

  // Path to the file where we can read and write data (in profile).
  base::ImportantFileWriter writer_;

  // Sequenced task runner where file I/O operations will be performed at.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  BackupState backup_state_ = BACKUP_NONE;

  base::WeakPtrFactory<MenuStorage> weak_factory_;
};

}  // namespace menus

#endif  // MENUS_MENU_STORAGE_H_
