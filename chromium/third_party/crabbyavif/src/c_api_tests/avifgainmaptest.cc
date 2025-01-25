// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <string>

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
  decoder->imageContentToDecode |= AVIF_IMAGE_CONTENT_GAIN_MAP;

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
  EXPECT_NE(decoder->image->gainMap, nullptr);
  // Color+alpha: 4x3 grid of 128x200 tiles.
  EXPECT_EQ(decoded->width, 128u * 4u);
  EXPECT_EQ(decoded->height, 200u * 3u);
  EXPECT_EQ(decoded->depth, 10u);
  ASSERT_NE(decoded->gainMap->image, nullptr);
  // Gain map: 2x2 grid of 64x80 tiles.
  EXPECT_EQ(decoded->gainMap->image->width, 64u * 2u);
  EXPECT_EQ(decoded->gainMap->image->height, 80u * 2u);
  EXPECT_EQ(decoded->gainMap->image->depth, 8u);
  EXPECT_EQ(decoded->gainMap->baseHdrHeadroom.n, 6u);
  EXPECT_EQ(decoded->gainMap->baseHdrHeadroom.d, 2u);

  // Decode the image.
  result = avifDecoderNextImage(decoder.get());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;
}

TEST(GainMapTest, DecodeOriented) {
  const std::string path = std::string(data_path) + "gainmap_oriented.avif";
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->imageContentToDecode |= AVIF_IMAGE_CONTENT_GAIN_MAP;
  ASSERT_EQ(avifDecoderSetIOFile(decoder.get(), path.c_str()), AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);

  // Verify that the transformative properties were kept.
  EXPECT_EQ(decoder->image->transformFlags,
            AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR);
  EXPECT_EQ(decoder->image->irot.angle, 1);
  EXPECT_EQ(decoder->image->imir.axis, 0);
  EXPECT_EQ(decoder->image->gainMap->image->transformFlags,
            AVIF_TRANSFORM_NONE);
}

TEST(GainMapTest, IgnoreGainMapButReadMetadata) {
  const std::string path =
      std::string(data_path) + "seine_sdr_gainmap_srgb.avif";
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);

  avifResult result = avifDecoderSetIOFile(decoder.get(), path.c_str());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;
  result = avifDecoderParse(decoder.get());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;
  avifImage* decoded = decoder->image;
  ASSERT_NE(decoded, nullptr);

  // Verify that the gain map was detected...
  EXPECT_NE(decoder->image->gainMap, nullptr);
  // ... but not decoded because enableDecodingGainMap is false by default.
  EXPECT_EQ(decoded->gainMap->image, nullptr);
  // Check that the gain map metadata WAS populated.
  EXPECT_EQ(decoded->gainMap->alternateHdrHeadroom.n, 13);
  EXPECT_EQ(decoded->gainMap->alternateHdrHeadroom.d, 10);
}

TEST(GainMapTest, IgnoreColorAndAlpha) {
  const std::string path =
      std::string(data_path) + "seine_sdr_gainmap_srgb.avif";
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->imageContentToDecode = AVIF_IMAGE_CONTENT_GAIN_MAP;

  avifResult result = avifDecoderSetIOFile(decoder.get(), path.c_str());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;
  result = avifDecoderParse(decoder.get());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;
  result = avifDecoderNextImage(decoder.get());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;
  avifImage* decoded = decoder->image;
  ASSERT_NE(decoded, nullptr);

  // Main image metadata is available.
  EXPECT_EQ(decoded->width, 400u);
  EXPECT_EQ(decoded->height, 300u);
  // But pixels are not.
  EXPECT_EQ(decoded->yuvRowBytes[0], 0u);
  EXPECT_EQ(decoded->yuvRowBytes[1], 0u);
  EXPECT_EQ(decoded->yuvRowBytes[2], 0u);
  EXPECT_EQ(decoded->alphaRowBytes, 0u);
  // The gain map was decoded.
  EXPECT_NE(decoder->image->gainMap, nullptr);
  ASSERT_NE(decoded->gainMap->image, nullptr);
  // Including pixels.
  EXPECT_GT(decoded->gainMap->image->yuvRowBytes[0], 0u);
}

TEST(GainMapTest, IgnoreAll) {
  const std::string path =
      std::string(data_path) + "seine_sdr_gainmap_srgb.avif";
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->imageContentToDecode = AVIF_IMAGE_CONTENT_NONE;

  avifResult result = avifDecoderSetIOFile(decoder.get(), path.c_str());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;
  result = avifDecoderParse(decoder.get());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;
  avifImage* decoded = decoder->image;
  ASSERT_NE(decoded, nullptr);

  EXPECT_NE(decoder->image->gainMap, nullptr);
  ASSERT_EQ(decoder->image->gainMap->image, nullptr);

  // But trying to access the next image should give an error because both
  // ignoreColorAndAlpha and enableDecodingGainMap are set.
  ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_NO_CONTENT);
}

// The following two functions use avifDecoderReadFile which is not supported in
// CAPI yet.

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
  EXPECT_EQ(decoded->gainMap->baseHdrHeadroom.n, 6u);
  EXPECT_EQ(decoded->gainMap->baseHdrHeadroom.d, 2u);
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
  EXPECT_EQ(decoded->gainMap->baseHdrHeadroom.n, 6u);
  EXPECT_EQ(decoded->gainMap->baseHdrHeadroom.d, 2u);
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
