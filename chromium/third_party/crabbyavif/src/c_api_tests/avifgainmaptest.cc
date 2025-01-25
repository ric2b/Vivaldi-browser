// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

TEST(GainMapTest, DecodeGainMapGrid) {
  const std::string path =
      std::string(data_path) + "color_grid_gainmap_different_grid.avif";
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->enableDecodingGainMap = true;
  decoder->enableParsingGainMapMetadata = true;

  avifResult result = avifDecoderSetIOFile(decoder.get(), path.c_str());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;

  // Just parse the image first.
  result = avifDecoderParse(decoder.get());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;
  avifImage* decoded = decoder->image;
  ASSERT_NE(decoded, nullptr);

  // Verify that the gain map is present and matches the input.
  EXPECT_TRUE(decoder->gainMapPresent);
  // Color+alpha: 4x3 grid of 128x200 tiles.
  EXPECT_EQ(decoded->width, 128u * 4u);
  EXPECT_EQ(decoded->height, 200u * 3u);
  EXPECT_EQ(decoded->depth, 10u);
  ASSERT_NE(decoded->gainMap, nullptr);
  ASSERT_NE(decoded->gainMap->image, nullptr);
  // Gain map: 2x2 grid of 64x80 tiles.
  EXPECT_EQ(decoded->gainMap->image->width, 64u * 2u);
  EXPECT_EQ(decoded->gainMap->image->height, 80u * 2u);
  EXPECT_EQ(decoded->gainMap->image->depth, 8u);
  EXPECT_EQ(decoded->gainMap->metadata.alternateHdrHeadroomN, 6u);
  EXPECT_EQ(decoded->gainMap->metadata.alternateHdrHeadroomD, 2u);

  // Decode the image.
  result = avifDecoderNextImage(decoder.get());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;
}

// The following two functions use avifDecoderReadFile which is not supported in CAPI yet.

/*
TEST(GainMapTest, DecodeColorGridGainMapNoGrid) {
  const std::string path =
      std::string(data_path) + "color_grid_alpha_grid_gainmap_nogrid.avif";
  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->enableDecodingGainMap = true;
  decoder->enableParsingGainMapMetadata = true;
  ASSERT_EQ(avifDecoderReadFile(decoder.get(), decoded.get(), path.c_str()),
            AVIF_RESULT_OK);

  // Color+alpha: 4x3 grid of 128x200 tiles.
  EXPECT_EQ(decoded->width, 128u * 4u);
  EXPECT_EQ(decoded->height, 200u * 3u);
  ASSERT_NE(decoded->gainMap, nullptr);
  ASSERT_NE(decoded->gainMap->image, nullptr);
  // Gain map: single image of size 64x80.
  EXPECT_EQ(decoded->gainMap->image->width, 64u);
  EXPECT_EQ(decoded->gainMap->image->height, 80u);
  EXPECT_EQ(decoded->gainMap->metadata.alternateHdrHeadroomN, 6u);
  EXPECT_EQ(decoded->gainMap->metadata.alternateHdrHeadroomD, 2u);
}

TEST(GainMapTest, DecodeColorNoGridGainMapGrid) {
  const std::string path =
      std::string(data_path) + "color_nogrid_alpha_nogrid_gainmap_grid.avif";
  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->enableDecodingGainMap = true;
  decoder->enableParsingGainMapMetadata = true;
  ASSERT_EQ(avifDecoderReadFile(decoder.get(), decoded.get(), path.c_str()),
            AVIF_RESULT_OK);

  // Color+alpha: single image of size 128x200 .
  EXPECT_EQ(decoded->width, 128u);
  EXPECT_EQ(decoded->height, 200u);
  ASSERT_NE(decoded->gainMap, nullptr);
  ASSERT_NE(decoded->gainMap->image, nullptr);
  // Gain map: 2x2 grid of 64x80 tiles.
  EXPECT_EQ(decoded->gainMap->image->width, 64u * 2u);
  EXPECT_EQ(decoded->gainMap->image->height, 80u * 2u);
  EXPECT_EQ(decoded->gainMap->metadata.alternateHdrHeadroomN, 6u);
  EXPECT_EQ(decoded->gainMap->metadata.alternateHdrHeadroomD, 2u);
}
*/

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
