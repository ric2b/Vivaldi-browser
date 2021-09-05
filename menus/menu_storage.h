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
}

namespace content {
class BrowserContext;
}

namespace menus {

struct Menu_Control;
class Menu_Model;
class Menu_Node;


class MenuLoadDetails {
 public:
  explicit MenuLoadDetails(Menu_Node* mainmenu, Menu_Control* control,
                           int64_t id, bool force_bundle);
  explicit MenuLoadDetails(Menu_Node* mainmenu, Menu_Control* control,
                           const std::string& menu, bool force_bundle);
  ~MenuLoadDetails();

  void SetHasUpgraded();

  Menu_Node* mainmenu_node() const { return mainmenu_node_.get(); }
  Menu_Control* control() const { return control_.get(); }

  int64_t id() { return id_; }
  const std::string& menu() { return menu_; }
  bool has_upgraded() const { return has_upgraded_; }
  bool force_bundle() const { return force_bundle_; }
  std::unique_ptr<Menu_Node> release_mainmenu_node() {
    return std::move(mainmenu_node_);
  }
  std::unique_ptr<Menu_Control> release_control() {
    return std::move(control_);
  }

 private:
  std::unique_ptr<Menu_Node> mainmenu_node_;
  std::unique_ptr<Menu_Control> control_;
  int64_t id_;
  bool has_upgraded_;
  bool force_bundle_;
  std::string menu_;

  DISALLOW_COPY_AND_ASSIGN(MenuLoadDetails);
};

class MenuStorage : public base::ImportantFileWriter::DataSerializer {
 public:
  MenuStorage(content::BrowserContext* context,
               Menu_Model* model,
               base::SequencedTaskRunner* sequenced_task_runner);

  ~MenuStorage() override;

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
  bool SerializeData(std::string* output) override;

  bool SaveValue(const base::Value& value);
 private:
  // Backup is done once and only if a regular save is attempted.
  enum BackupState {
    BACKUP_NONE,        // No backup attempted
    BACKUP_DISPATCHED,  // Request posted
    BACKUP_ATTEMPTED    // Backup has been called.
  };

  friend class base::RefCountedThreadSafe<MenuStorage>;

  void OnBackupFinished();

  // Serializes the data and schedules save using ImportantFileWriter.
  // Returns true on successful serialization.
  bool SaveNow();

  Menu_Model* model_;

  // Path to the file where we can read and write data (in profile).
  base::ImportantFileWriter writer_;

  // The bundled path. We use this if the profile path does not exits
  base::FilePath bundled_file_;

  // Sequenced task runner where file I/O operations will be performed at.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  BackupState backup_state_ = BACKUP_NONE;

  base::WeakPtrFactory<MenuStorage> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MenuStorage);
};

}  // namespace menus


#endif  // MENUS_MENU_STORAGE_H_
