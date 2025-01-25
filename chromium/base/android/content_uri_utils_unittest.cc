// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/content_uri_utils.h"

#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/test_file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

TEST(ContentUriUtilsTest, Test) {
  // Get the test image path.
  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &data_dir));
  data_dir = data_dir.AppendASCII("file_util");
  ASSERT_TRUE(PathExists(data_dir));
  FilePath image_file = data_dir.Append(FILE_PATH_LITERAL("red.png"));

  // Insert the image into MediaStore. MediaStore will do some conversions, and
  // return the content URI.
  FilePath path = base::InsertImageIntoMediaStore(image_file);
  EXPECT_TRUE(path.IsContentUri());
  EXPECT_TRUE(PathExists(path));

  // Validate GetContentUriMimeType().
  std::string mime = GetContentUriMimeType(path);
  EXPECT_EQ(mime, std::string("image/png"));

  // Validate GetContentUriFileSize().
  File::Info info;
  EXPECT_TRUE(GetFileInfo(path, &info));
  EXPECT_GT(info.size, 0);
  EXPECT_EQ(GetContentUriFileSize(path), info.size);

  FilePath invalid_path("content://foo.bar");
  mime = GetContentUriMimeType(invalid_path);
  EXPECT_TRUE(mime.empty());
  EXPECT_EQ(GetContentUriFileSize(invalid_path), -1);
}

TEST(ContentUriUtilsTest, TranslateOpenFlagsToJavaMode) {
  constexpr auto kTranslations = MakeFixedFlatMap<uint32_t, std::string>({
      {File::FLAG_OPEN | File::FLAG_READ, "r"},
      {File::FLAG_OPEN_ALWAYS | File::FLAG_READ | File::FLAG_WRITE, "rw"},
      {File::FLAG_OPEN_ALWAYS | File::FLAG_APPEND, "wa"},
      {File::FLAG_CREATE_ALWAYS | File::FLAG_READ | File::FLAG_WRITE, "rwt"},
      {File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE, "wt"},
  });

  for (const auto open_or_create : std::vector<uint32_t>(
           {0u, File::FLAG_OPEN, File::FLAG_CREATE, File::FLAG_OPEN_ALWAYS,
            File::FLAG_CREATE_ALWAYS, File::FLAG_OPEN_TRUNCATED})) {
    for (const auto read_write_append : std::vector<uint32_t>(
             {0u, File::FLAG_READ, File::FLAG_WRITE, File::FLAG_APPEND,
              File::FLAG_READ | File::FLAG_WRITE})) {
      for (const auto other : std::vector<uint32_t>(
               {0u, File::FLAG_DELETE_ON_CLOSE, File::FLAG_TERMINAL_DEVICE})) {
        uint32_t open_flags = open_or_create | read_write_append | other;
        auto mode = TranslateOpenFlagsToJavaMode(open_flags);
        auto it = kTranslations.find(open_flags);
        if (it != kTranslations.end()) {
          EXPECT_TRUE(mode.has_value()) << "flag=0x" << std::hex << open_flags;
          EXPECT_EQ(mode.value(), it->second)
              << "flag=0x" << std::hex << open_flags;
        } else {
          EXPECT_FALSE(mode.has_value()) << "flag=0x" << std::hex << open_flags;
        }
      }
    }
  }
}

}  // namespace android
}  // namespace base
