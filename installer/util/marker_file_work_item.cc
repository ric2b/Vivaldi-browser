// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "installer/util/marker_file_work_item.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/win/scoped_handle.h"

namespace vivaldi {

MarkerFileWorkItem::MarkerFileWorkItem(base::FilePath dest_path,
                                       std::string initial_text)
    : dest_path_(std::move(dest_path)), initial_text_(std::move(initial_text)) {
  DCHECK(initial_text.size() <= static_cast<DWORD>(-1));
}

MarkerFileWorkItem::~MarkerFileWorkItem() = default;

bool MarkerFileWorkItem::DoImpl() {
  base::win::ScopedHandle file(
      ::CreateFile(dest_path_.value().c_str(), GENERIC_WRITE, 0, nullptr,
                   CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!file.IsValid()) {
    if (::GetLastError() == ERROR_FILE_EXISTS) {
      // Marker already exists. We are done.
      VLOG(1) << "Marker file " << dest_path_ << " already exists.";
      return true;
    }
    PLOG(ERROR) << "CreateFile failed for path " << dest_path_;
    return false;
  }

  DWORD size = initial_text_.size();
  DWORD written = 0;
  BOOL result =
      ::WriteFile(file.Get(), initial_text_.data(), size, &written, nullptr);
  if (result && written == size) {
    created_ = true;
    VLOG(1) << "Created new " << dest_path_;
    return true;
  }

  if (!result) {
    // WriteFile failed.
    PLOG(ERROR) << "writing file " << dest_path_ << " failed";
  } else {
    // Didn't write all the bytes.
    LOG(ERROR) << "wrote" << written << " bytes to " << dest_path_
               << " expected " << size;
  }
  file.Close();
  DeleteDestination();
  return false;
}

void MarkerFileWorkItem::RollbackImpl() {
  if (created_) {
    DeleteDestination();
  }
}

void MarkerFileWorkItem::DeleteDestination() {
  VLOG(1) << "Deleting " << dest_path_;
  if (!::DeleteFile(dest_path_.value().c_str())) {
    PLOG(ERROR) << "failed to delete " << dest_path_;
  }
}

}  // namespace vivaldi
