// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be

#include "gn/bundle_data.h"
#include "util/test/test.h"

TEST(BundleDataTest, GetAssetsCatalogDirectory) {
  struct TestCase {
    SourceFile source_file;
    SourceFile catalog_dir;
  };

  static TestCase kTestCases[] = {
      {
          .source_file = SourceFile("//my/bundle/foo.xcassets/my/file"),
          .catalog_dir = SourceFile("//my/bundle/foo.xcassets"),
      },
      {
          .source_file = SourceFile(
              "//my/bundle/foo.xcassets/nested/bar.xcassets/my/file"),
          .catalog_dir = SourceFile("//my/bundle/foo.xcassets"),
      },
      {
          .source_file = SourceFile("//my/bundle/my/file"),
          .catalog_dir = SourceFile(),
      },
  };

  for (const TestCase& test_case : kTestCases) {
    const SourceFile assets_catalog_dir =
        BundleData::GetAssetsCatalogDirectory(test_case.source_file);
    EXPECT_EQ(assets_catalog_dir, test_case.catalog_dir);
  }
}
