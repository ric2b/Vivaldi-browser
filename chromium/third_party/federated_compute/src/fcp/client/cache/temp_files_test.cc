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

#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/test_helpers.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace client {
namespace cache {
namespace {

int CountFilesInDir(const std::filesystem::path& dir) {
  int num_files = 0;
  for ([[maybe_unused]] auto const& unused :
       std::filesystem::directory_iterator{dir}) {
    num_files++;
  }
  return num_files;
}

class TempFilesTest : public testing::Test {
 protected:
  void SetUp() override {
    root_dir_ = testing::TempDir();
    std::filesystem::path root_dir(root_dir_);
    temp_file_dir_ =
        root_dir / TempFiles::kParentDir / TempFiles::kTempFilesDir;
  }
  void TearDown() override {
    std::filesystem::remove_all(std::filesystem::path(root_dir_));
  }

  testing::StrictMock<MockLogManager> log_manager_;
  std::string root_dir_;
  std::filesystem::path temp_file_dir_;
};

TEST_F(TempFilesTest, FailToCreateParentDirectory) {
  ASSERT_THAT(TempFiles::Create("/proc/0", &log_manager_), IsCode(INTERNAL));
}

TEST_F(TempFilesTest, InvalidRelativePath) {
  ASSERT_THAT(TempFiles::Create("relative/cache", &log_manager_),
              IsCode(INVALID_ARGUMENT));
}

TEST_F(TempFilesTest, SuccessfulInitialization) {
  ASSERT_OK(TempFiles::Create(root_dir_, &log_manager_));
}

TEST_F(TempFilesTest, CreateTempFile) {
  auto temp_files = TempFiles::Create(root_dir_, &log_manager_);
  ASSERT_OK(temp_files);
  auto temp_file = (*temp_files)->CreateTempFile("stefan", ".cool");
  ASSERT_OK(temp_file);
}

TEST_F(TempFilesTest, CreateSomeTempFilesThenDeleteInDtor) {
  auto temp_files = TempFiles::Create(root_dir_, &log_manager_);
  ASSERT_OK(temp_files);
  int num_temp_files = 4;
  for (int i = 0; i < num_temp_files; i++) {
    ASSERT_OK((*temp_files)->CreateTempFile("stefan", ".cool"));
  }
  ASSERT_EQ(num_temp_files, CountFilesInDir(temp_file_dir_));

  temp_files->reset();  // deleting temp_files should empty the directory.

  ASSERT_EQ(0, CountFilesInDir(temp_file_dir_));
}

TEST_F(TempFilesTest, CreatingTempFilesDeletesExistingFiles) {
  std::filesystem::path root_dir(root_dir_);

  ASSERT_TRUE(std::filesystem::create_directories(temp_file_dir_));

  int num_existing_temp_files = 10;
  for (int i = 0; i < num_existing_temp_files; i++) {
    std::filesystem::path temp_file_path =
        temp_file_dir_ / absl::StrCat("temp", i);
    std::ofstream{temp_file_path};
  }
  ASSERT_EQ(num_existing_temp_files, CountFilesInDir(temp_file_dir_));

  auto temp_files = TempFiles::Create(root_dir_, &log_manager_);
  ASSERT_OK(temp_files);
  ASSERT_EQ(0, CountFilesInDir(temp_file_dir_));
}

TEST_F(TempFilesTest, FailToDeleteTempFilesLogs) {
  // Create a temp file in the temp dir
  auto temp_files = TempFiles::Create(root_dir_, &log_manager_);
  ASSERT_OK(temp_files);
  ASSERT_OK((*temp_files)->CreateTempFile("stefan", ".cool"));
  ASSERT_OK((*temp_files)->CreateTempFile("stefan", ".cool"));
  ASSERT_OK((*temp_files)->CreateTempFile("stefan", ".cool"));
  ASSERT_EQ(3, CountFilesInDir(temp_file_dir_));

  // Delete the temp file dir and root dir, which should cause the dtor to fail
  // because we deleted the directories out from underneath it.
  std::filesystem::remove_all(std::filesystem::path(root_dir_));

  EXPECT_CALL(log_manager_,
              LogDiag(ProdDiagCode::TEMP_FILES_NATIVE_FAILED_TO_DELETE));
  temp_files->reset();
}

}  // namespace
}  // namespace cache
}  // namespace client
}  // namespace fcp
