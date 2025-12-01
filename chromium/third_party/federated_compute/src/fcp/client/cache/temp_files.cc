/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fcp/client/cache/temp_files.h"

#include <sys/file.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <memory>
#include <string>
#include <system_error>  // NOLINT

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/log_manager.h"

namespace fcp {
namespace client {
namespace cache {
namespace {

absl::Status DeleteFilesInDirectory(const std::filesystem::path& directory) {
  if (!std::filesystem::exists(directory)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Directory does not exist: ", directory.string()));
  }
  absl::Status status = absl::OkStatus();
  // Note this only iterates through the top level directory and will not
  // traverse subdirectories.
  for (auto& de : std::filesystem::directory_iterator(directory)) {
    std::error_code error;
    // Save the first error, but attempt to delete the other files.
    if (!std::filesystem::remove(de.path(), error)) {
      if (status.ok()) {
        status = absl::InternalError(absl::StrCat(
            "Failed to delete file with error code: ", error.value()));
      }
    }
  }
  return status;
}

}  // namespace

absl::StatusOr<std::unique_ptr<TempFiles>> TempFiles::Create(
    const std::string& cache_dir, LogManager* log_manager) {
  std::filesystem::path root_path(cache_dir);
  if (!root_path.is_absolute()) {
    return absl::InvalidArgumentError(
        absl::StrCat("The provided path: ", cache_dir,
                     "is invalid. The path must start with \"/\""));
  }

  // Create fcp parent dir in the passed root dir.
  std::filesystem::path fcp_base_dir = root_path / kParentDir;
  std::error_code error;
  std::filesystem::create_directories(fcp_base_dir, error);
  if (error.value() != 0) {
    return absl::InternalError(absl::StrCat(
        "Failed to create TempFiles base directory ",
        fcp_base_dir.generic_string(), " with error code ", error.value()));
  }

  // Create directory in parent dir for temporary files.
  std::filesystem::path temp_files_dir = fcp_base_dir / kTempFilesDir;
  std::filesystem::create_directories(temp_files_dir, error);
  if (error.value() != 0) {
    return absl::InternalError(
        absl::StrCat("Failed to create TempFiles temp file directory ",
                     temp_files_dir.generic_string()));
  }

  // We clean up the temp files dir on creation in case we failed to clean it up
  // during a previous run (i.e. due to the training process getting killed
  // etc.) and to make sure we don't end up in the pathological case where we
  // are always crashing partway through training and stranding temp files
  // because the TempFiles dtor never runs.
  auto cleanup_status = DeleteFilesInDirectory(temp_files_dir);
  if (!cleanup_status.ok()) {
    log_manager->LogDiag(ProdDiagCode::TEMP_FILES_NATIVE_FAILED_TO_DELETE);
    return cleanup_status;
  }
  return absl::WrapUnique(new TempFiles(temp_files_dir, log_manager));
}

absl::StatusOr<std::string> TempFiles::CreateTempFile(
    const std::string& prefix, const std::string& suffix) {
  std::filesystem::path candidate_path;
  int fd;
  do {
    candidate_path = temp_files_dir_ /
                     absl::StrCat(prefix, std::to_string(std::rand()), suffix);
  } while ((fd = open(candidate_path.c_str(), O_CREAT | O_EXCL | O_RDWR,
                      S_IRWXU)) == -1 &&
           errno == EEXIST);
  close(fd);
  std::ofstream tmp_file(candidate_path);
  if (!tmp_file) {
    return absl::InvalidArgumentError(
        absl::StrCat("could not create file ", candidate_path.string()));
  }

  return candidate_path.string();
}

TempFiles::~TempFiles() {
  if (!DeleteFilesInDirectory(temp_files_dir_).ok()) {
    log_manager_.LogDiag(ProdDiagCode::TEMP_FILES_NATIVE_FAILED_TO_DELETE);
  }
}

}  // namespace cache
}  // namespace client
}  // namespace fcp
