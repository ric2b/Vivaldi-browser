// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

// A test for https://github.com/AOMediaCodec/libavif/issues/1086 to prevent
// regression.
TEST(MetadataTest, DecoderParseICC) {
  std::string file_path = std::string(data_path) + "paris_icc_exif_xmp.avif";
  avifDecoder* decoder = avifDecoderCreate();
  ASSERT_NE(decoder, nullptr);
  EXPECT_EQ(avifDecoderSetIOFile(decoder, file_path.c_str()), AVIF_RESULT_OK);

  decoder->ignoreXMP = AVIF_TRUE;
  decoder->ignoreExif = AVIF_TRUE;
  EXPECT_EQ(avifDecoderParse(decoder), AVIF_RESULT_OK);

  ASSERT_GE(decoder->image->icc.size, 4u);
  EXPECT_EQ(decoder->image->icc.data[0], 0);
  EXPECT_EQ(decoder->image->icc.data[1], 0);
  EXPECT_EQ(decoder->image->icc.data[2], 2);
  EXPECT_EQ(decoder->image->icc.data[3], 84);

  ASSERT_EQ(decoder->image->exif.size, 0u);
  ASSERT_EQ(decoder->image->xmp.size, 0u);

  decoder->ignoreXMP = AVIF_FALSE;
  decoder->ignoreExif = AVIF_FALSE;
  EXPECT_EQ(avifDecoderParse(decoder), AVIF_RESULT_OK);

  ASSERT_GE(decoder->image->exif.size, 4u);
  EXPECT_EQ(decoder->image->exif.data[0], 73);
  EXPECT_EQ(decoder->image->exif.data[1], 73);
  EXPECT_EQ(decoder->image->exif.data[2], 42);
  EXPECT_EQ(decoder->image->exif.data[3], 0);

  ASSERT_GE(decoder->image->xmp.size, 4u);
  EXPECT_EQ(decoder->image->xmp.data[0], 60);
  EXPECT_EQ(decoder->image->xmp.data[1], 63);
  EXPECT_EQ(decoder->image->xmp.data[2], 120);
  EXPECT_EQ(decoder->image->xmp.data[3], 112);

  avifDecoderDestroy(decoder);
}

}  // namespace
}  // namespace avif

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "There must be exactly one argument containing the path to "
                 "the test data folder"
              << std::endl;
    return 1;
  }
  avif::data_path = argv[1];
  return RUN_ALL_TESTS();
}
