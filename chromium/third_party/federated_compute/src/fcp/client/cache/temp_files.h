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

#ifndef FCP_CLIENT_CACHE_TEMP_FILES_H_
#define FCP_CLIENT_CACHE_TEMP_FILES_H_

#include <filesystem>  // NOLINT(build/c++17)
#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "fcp/client/files.h"
#include "fcp/client/log_manager.h"

namespace fcp {
namespace client {
namespace cache {

// Manages temporary files created by the federated compute runtime. Unlike
// other Files implementations, TempFiles will clean up created temporary files
// eagerly as part its construction and deletion.
class TempFiles : public Files {
 public:
  static constexpr char kParentDir[] = "fcp";
  // The subdirectory temporary files will be created in. Files in this
  // directory are deleted at the end of a federated computation.
  static constexpr char kTempFilesDir[] = "tmp";

  // Factory method to create TempFiles. The provided cache dir is the
  // absolute path for storing cached files. TempFiles will attempt to
  // create subdirectories and files, so the directory must grant read/write
  // access.
  static absl::StatusOr<std::unique_ptr<TempFiles>> Create(
      const std::string& cache_dir, LogManager* log_manager);

  // Creates a temporary file. TempFiles will delete these files at the end
  // of a federated computation run, or upon the next creation of a TempFiles
  // instance.
  // On success, returns a file path.
  // On error, returns
  // - INTERNAL - unexpected error.
  // - INVALID_ARGUMENT - on "expected" errors such as I/O issues.
  absl::StatusOr<std::string> CreateTempFile(
      const std::string& prefix, const std::string& suffix) override;

  // Any temporary Files created with TempFiles will be deleted.
  ~TempFiles() override;

  // TempFiles is neither copyable nor movable.
  TempFiles(const TempFiles&) = delete;
  TempFiles& operator=(const TempFiles&) = delete;

 private:
  TempFiles(std::filesystem::path temp_files_dir, LogManager* log_manager)
      : temp_files_dir_(temp_files_dir), log_manager_(*log_manager) {}

  const std::filesystem::path temp_files_dir_;
  LogManager& log_manager_;
};

}  // namespace cache
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_CACHE_TEMP_FILES_H_
