/*
 * Copyright 2020 The Chromium Authors. All Rights Reserved.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "gtest-utils.h"

#include <gtest/gtest.h>
#include <string>

extern "C" int tjbench(int argc, char *argv[]);

// Test image files and their expected MD5 sums.
const static std::vector<std::pair<const std::string,
                                   const std::string>> IMAGE_MD5_BASELINE = {
  { "testout_tile_GRAY_Q95_8x8.ppm", "2c3b567086e6ca0c5e6d34ad8d6f6fe8" },
  { "testout_tile_420_Q95_8x8.ppm", "efca1bdf0226df01777137778cf986ec" },
  { "testout_tile_422_Q95_8x8.ppm", "c300553ce1b3b90fd414ec96b62fe988" },
  { "testout_tile_444_Q95_8x8.ppm", "87bd58005eec73f0f313c8e38d0d793c" },
  { "testout_tile_GRAY_Q95_16x16.ppm", "2c3b567086e6ca0c5e6d34ad8d6f6fe8" },
  { "testout_tile_420_Q95_16x16.ppm", "8c92c7453870d9e11c6d1dec3a8c9101" },
  { "testout_tile_422_Q95_16x16.ppm", "6559ddb1191f5b2d3eb41081b254c4e0" },
  { "testout_tile_444_Q95_16x16.ppm", "87bd58005eec73f0f313c8e38d0d793c" },
  { "testout_tile_GRAY_Q95_32x32.ppm", "2c3b567086e6ca0c5e6d34ad8d6f6fe8" },
  { "testout_tile_420_Q95_32x32.ppm", "3f7651872a95e469d1c7115f1b11ecef" },
  { "testout_tile_422_Q95_32x32.ppm", "58691797f4584c4c5ed5965a6bb9aec0" },
  { "testout_tile_444_Q95_32x32.ppm", "87bd58005eec73f0f313c8e38d0d793c" },
  { "testout_tile_GRAY_Q95_64x64.ppm", "2c3b567086e6ca0c5e6d34ad8d6f6fe8" },
  { "testout_tile_420_Q95_64x64.ppm", "f64c71af03fdea12363b62f1a3096aab" },
  { "testout_tile_422_Q95_64x64.ppm", "7f9e34942ae46af7b784f459ec133f5e" },
  { "testout_tile_444_Q95_64x64.ppm", "87bd58005eec73f0f313c8e38d0d793c" },
  { "testout_tile_GRAY_Q95_128x128.ppm", "2c3b567086e6ca0c5e6d34ad8d6f6fe8" },
  { "testout_tile_420_Q95_128x128.ppm", "5a5ef57517558c671bf5e75793588d69" },
  { "testout_tile_422_Q95_128x128.ppm", "6afcb77580d85dd3eacb04b3c2bc7710" },
  { "testout_tile_444_Q95_128x128.ppm", "87bd58005eec73f0f313c8e38d0d793c" },
};

class TJBenchTest : public
  ::testing::TestWithParam<std::pair<const std::string, const std::string>> {

 protected:

  static void SetUpTestSuite() {
    base::FilePath resource_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &resource_path));
    resource_path = resource_path.AppendASCII("third_party");
    resource_path = resource_path.AppendASCII("libjpeg_turbo");
    resource_path = resource_path.AppendASCII("testimages");
    resource_path = resource_path.AppendASCII("testorig.ppm");
    ASSERT_TRUE(base::PathExists(resource_path));

    base::FilePath target_path(GetTargetDirectory());
    target_path = target_path.AppendASCII("testout_tile.ppm");

    ASSERT_TRUE(base::CopyFile(resource_path, target_path));

    std::string prog_name = "tjbench";
    std::string arg1 = target_path.MaybeAsASCII();
    std::string arg2 = "95";
    std::string arg3 = "-rgb";
    std::string arg4 = "-quiet";
    std::string arg5 = "-tile";
    std::string arg6 = "-benchtime";
    std::string arg7 = "0.01";
    std::string arg8 = "-warmup";
    std::string arg9 = "0";
    char *command_line[] = { &prog_name[0],
                             &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                             &arg6[0], &arg7[0], &arg8[0], &arg9[0],
                           };
    // Generate test image tiles.
    EXPECT_EQ(tjbench(10, command_line), 0);
  }

};

TEST_P(TJBenchTest, TestTileBaseline) {
  // Construct path for test image file.
  base::FilePath test_image_path(GetTargetDirectory());
  test_image_path = test_image_path.AppendASCII(std::get<0>(GetParam()));
  // Read test image as string and compute MD5 sum.
  std::string test_image_data;
  ASSERT_TRUE(base::ReadFileToString(test_image_path, &test_image_data));
  const std::string md5 = base::MD5String(test_image_data);
  // Compare expected MD5 sum against that of test image.
  EXPECT_EQ(std::get<1>(GetParam()), md5);
}

INSTANTIATE_TEST_SUITE_P(TestTileBaseline,
                         TJBenchTest,
                         ::testing::ValuesIn(IMAGE_MD5_BASELINE));

// Test image files and their expected MD5 sums.
const static std::vector<std::pair<const std::string,
                                   const std::string>> IMAGE_MD5_MERGED = {
  { "testout_tilem_420_Q95_8x8.ppm", "66bd869b315a32a00fef1a025661ce72" },
  { "testout_tilem_422_Q95_8x8.ppm", "55df1f96bcfb631aedeb940cf3f011f5" },
  { "testout_tilem_420_Q95_16x16.ppm", "bf9ec2ab4875abb2efcce8f876fe2c2a" },
  { "testout_tilem_422_Q95_16x16.ppm", "6502031018c2d2f69bc6353347f8df4d" },
  { "testout_tilem_420_Q95_32x32.ppm", "bf9ec2ab4875abb2efcce8f876fe2c2a" },
  { "testout_tilem_422_Q95_32x32.ppm", "6502031018c2d2f69bc6353347f8df4d" },
  { "testout_tilem_420_Q95_64x64.ppm", "bf9ec2ab4875abb2efcce8f876fe2c2a" },
  { "testout_tilem_422_Q95_64x64.ppm", "6502031018c2d2f69bc6353347f8df4d" },
  { "testout_tilem_420_Q95_128x128.ppm", "bf9ec2ab4875abb2efcce8f876fe2c2a" },
  { "testout_tilem_422_Q95_128x128.ppm", "6502031018c2d2f69bc6353347f8df4d" },
};

class TJBenchTestMerged : public
  ::testing::TestWithParam<std::pair<const std::string, const std::string>> {

 protected:

  static void SetUpTestSuite() {
    base::FilePath resource_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &resource_path));
    resource_path = resource_path.AppendASCII("third_party");
    resource_path = resource_path.AppendASCII("libjpeg_turbo");
    resource_path = resource_path.AppendASCII("testimages");
    resource_path = resource_path.AppendASCII("testorig.ppm");
    ASSERT_TRUE(base::PathExists(resource_path));

    base::FilePath target_path(GetTargetDirectory());
    target_path = target_path.AppendASCII("testout_tilem.ppm");

    ASSERT_TRUE(base::CopyFile(resource_path, target_path));

    std::string prog_name = "tjbench";
    std::string arg1 = target_path.MaybeAsASCII();
    std::string arg2 = "95";
    std::string arg3 = "-rgb";
    std::string arg4 = "-fastupsample";
    std::string arg5 = "-quiet";
    std::string arg6 = "-tile";
    std::string arg7 = "-benchtime";
    std::string arg8 = "0.01";
    std::string arg9 = "-warmup";
    std::string arg10 = "0";
    char *command_line[] = { &prog_name[0],
                             &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                             &arg6[0], &arg7[0], &arg8[0], &arg9[0], &arg10[0]
                           };
    // Generate test image output tiles.
    EXPECT_EQ(tjbench(11, command_line), 0);
  }

};

TEST_P(TJBenchTestMerged, TestTileMerged) {
  // Construct path for test image file.
  base::FilePath test_image_path(GetTargetDirectory());
  test_image_path = test_image_path.AppendASCII(std::get<0>(GetParam()));
  // Read test image as string and compute MD5 sum.
  std::string test_image_data;
  ASSERT_TRUE(base::ReadFileToString(test_image_path, &test_image_data));
  const std::string md5 = base::MD5String(test_image_data);
  // Compare expected MD5 sum against that of test image.
  EXPECT_EQ(std::get<1>(GetParam()), md5);
}

INSTANTIATE_TEST_SUITE_P(TestTileMerged,
                         TJBenchTestMerged,
                         ::testing::ValuesIn(IMAGE_MD5_MERGED));
