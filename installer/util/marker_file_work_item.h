// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef INSTALLER_UTIL_MARKER_FILE_WORK_ITEM_H_
#define INSTALLER_UTIL_MARKER_FILE_WORK_ITEM_H_

#include "base/files/file_path.h"
#include "chrome/installer/util/work_item.h"

namespace vivaldi {

// A WorkItem subclass that creates a marker file which presence alone marks the
// installation in certain way and which contents is irrelevant.
class MarkerFileWorkItem : public WorkItem {
 public:
  MarkerFileWorkItem(base::FilePath dest_path, std::string initial_text);
  ~MarkerFileWorkItem() override;

 private:
  void DeleteDestination();
  // WorkItem:
  bool DoImpl() override;
  void RollbackImpl() override;

  // Destination path to create the marker at.
  base::FilePath dest_path_;

  std::string initial_text_;

  // True if DoImpl created the file.
  bool created_ = false;
};

}  // namespace vivaldi

#endif  // INSTALLER_UTIL_MARKER_FILE_WORK_ITEM_H_
