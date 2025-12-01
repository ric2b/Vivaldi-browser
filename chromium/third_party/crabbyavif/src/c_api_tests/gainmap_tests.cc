// Copyright 2025 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <algorithm>
#include <cstdint>
#include <iostream>

#include "avif/avif.h"
#include "gtest/gtest.h"
#include "testutil.h"

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

DecoderPtr CreateDecoder(const AvifRwData& encoded) {
  DecoderPtr decoder(avifDecoderCreate());
  if (decoder == nullptr ||
      avifDecoderSetIOMemory(decoder.get(), encoded.data, encoded.size) !=
          AVIF_RESULT_OK) {
    return nullptr;
  }
  return decoder;
}

void CheckGainMapMetadataMatches(const avifGainMap& lhs,
                                 const avifGainMap& rhs) {
  EXPECT_EQ(lhs.baseHdrHeadroom.n, rhs.baseHdrHeadroom.n);
  EXPECT_EQ(lhs.baseHdrHeadroom.d, rhs.baseHdrHeadroom.d);
  EXPECT_EQ(lhs.alternateHdrHeadroom.n, rhs.alternateHdrHeadroom.n);
  EXPECT_EQ(lhs.alternateHdrHeadroom.d, rhs.alternateHdrHeadroom.d);
  for (int c = 0; c < 3; ++c) {
    SCOPED_TRACE(c);
    EXPECT_EQ(lhs.baseOffset[c].n, rhs.baseOffset[c].n);
    EXPECT_EQ(lhs.baseOffset[c].d, rhs.baseOffset[c].d);
    EXPECT_EQ(lhs.alternateOffset[c].n, rhs.alternateOffset[c].n);
    EXPECT_EQ(lhs.alternateOffset[c].d, rhs.alternateOffset[c].d);
    EXPECT_EQ(lhs.gainMapGamma[c].n, rhs.gainMapGamma[c].n);
    EXPECT_EQ(lhs.gainMapGamma[c].d, rhs.gainMapGamma[c].d);
    EXPECT_EQ(lhs.gainMapMin[c].n, rhs.gainMapMin[c].n);
    EXPECT_EQ(lhs.gainMapMin[c].d, rhs.gainMapMin[c].d);
    EXPECT_EQ(lhs.gainMapMax[c].n, rhs.gainMapMax[c].n);
    EXPECT_EQ(lhs.gainMapMax[c].d, rhs.gainMapMax[c].d);
  }
}

void FillTestGainMapMetadata(bool base_rendition_is_hdr, avifGainMap* gainMap) {
  gainMap->useBaseColorSpace = true;
  gainMap->baseHdrHeadroom = {0, 1};
  gainMap->alternateHdrHeadroom = {6, 2};
  if (base_rendition_is_hdr) {
    std::swap(gainMap->baseHdrHeadroom, gainMap->alternateHdrHeadroom);
  }
  for (int c = 0; c < 3; ++c) {
    gainMap->baseOffset[c] = {10 * c, 1000};
    gainMap->alternateOffset[c] = {20 * c, 1000};
    gainMap->gainMapGamma[c] = {1, static_cast<uint32_t>(c + 1)};
    gainMap->gainMapMin[c] = {-1, static_cast<uint32_t>(c + 1)};
    gainMap->gainMapMax[c] = {10 + c + 1, static_cast<uint32_t>(c + 1)};
  }
}

ImagePtr CreateTestImageWithGainMap(bool base_rendition_is_hdr) {
  ImagePtr image = testutil::CreateImage(/*width=*/12, /*height=*/34,
                                         /*depth=*/10, AVIF_PIXEL_FORMAT_YUV420,
                                         AVIF_PLANES_ALL, AVIF_RANGE_FULL);
  if (image == nullptr) {
    return nullptr;
  }
  image->colorPrimaries = AVIF_COLOR_PRIMARIES_BT2020;
  image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT601;
  image->transferCharacteristics =
      (avifTransferCharacteristics)(base_rendition_is_hdr
                                        ? AVIF_TRANSFER_CHARACTERISTICS_PQ
                                        : AVIF_TRANSFER_CHARACTERISTICS_SRGB);
  testutil::FillImageGradient(image.get(), /*offset=*/0);
  ImagePtr gain_map = testutil::CreateImage(
      /*width=*/6, /*height=*/17, /*depth=*/8, AVIF_PIXEL_FORMAT_YUV420,
      AVIF_PLANES_YUV, AVIF_RANGE_FULL);
  if (gain_map == nullptr) {
    return nullptr;
  }
  gain_map->colorPrimaries = AVIF_COLOR_PRIMARIES_UNSPECIFIED;
  gain_map->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT709;
  gain_map->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED;
  testutil::FillImageGradient(gain_map.get(), /*offset=*/0);
  image->gainMap = avifGainMapCreate();
  if (image->gainMap == nullptr) {
    return nullptr;
  }
  image->gainMap->image = gain_map.release();  // 'image' now owns the gain map.
  FillTestGainMapMetadata(base_rendition_is_hdr, image->gainMap);

  if (base_rendition_is_hdr) {
    image->clli.maxCLL = 10;
    image->clli.maxPALL = 5;
    image->gainMap->altDepth = 8;
    image->gainMap->altPlaneCount = 3;
    image->gainMap->altColorPrimaries = AVIF_COLOR_PRIMARIES_BT601;
    image->gainMap->altTransferCharacteristics =
        AVIF_TRANSFER_CHARACTERISTICS_SRGB;
    image->gainMap->altMatrixCoefficients = AVIF_MATRIX_COEFFICIENTS_SMPTE2085;
  } else {
    image->gainMap->altCLLI.maxCLL = 10;
    image->gainMap->altCLLI.maxPALL = 5;
    image->gainMap->altDepth = 10;
    image->gainMap->altPlaneCount = 3;
    image->gainMap->altColorPrimaries = AVIF_COLOR_PRIMARIES_BT2020;
    image->gainMap->altTransferCharacteristics =
        AVIF_TRANSFER_CHARACTERISTICS_PQ;
    image->gainMap->altMatrixCoefficients = AVIF_MATRIX_COEFFICIENTS_SMPTE2085;
  }

  return image;
}

TEST(GainMapTest, EncodeDecodeBaseImageSdr) {
  ImagePtr image = CreateTestImageWithGainMap(/*base_rendition_is_hdr=*/false);
  ASSERT_NE(image, nullptr);

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  AvifRwData encoded;
  ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded),
            AVIF_RESULT_OK);

  auto decoder = CreateDecoder(encoded);
  ASSERT_NE(decoder, nullptr);
  decoder->imageContentToDecode |= AVIF_IMAGE_CONTENT_GAIN_MAP;

  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  avifImage* decoded = decoder->image;
  ASSERT_NE(decoded, nullptr);

  ASSERT_NE(decoded->gainMap, nullptr);
  ASSERT_NE(decoded->gainMap->image, nullptr);
  EXPECT_EQ(decoded->gainMap->image->matrixCoefficients,
            image->gainMap->image->matrixCoefficients);
  EXPECT_EQ(decoded->gainMap->altCLLI.maxCLL, image->gainMap->altCLLI.maxCLL);
  EXPECT_EQ(decoded->gainMap->altCLLI.maxPALL, image->gainMap->altCLLI.maxPALL);
  EXPECT_EQ(decoded->gainMap->altDepth, 10u);
  EXPECT_EQ(decoded->gainMap->altPlaneCount, 3u);
  EXPECT_EQ(decoded->gainMap->altColorPrimaries, AVIF_COLOR_PRIMARIES_BT2020);
  EXPECT_EQ(decoded->gainMap->altTransferCharacteristics,
            AVIF_TRANSFER_CHARACTERISTICS_PQ);
  EXPECT_EQ(decoded->gainMap->altMatrixCoefficients,
            AVIF_MATRIX_COEFFICIENTS_SMPTE2085);
  EXPECT_EQ(decoded->gainMap->image->width, image->gainMap->image->width);
  EXPECT_EQ(decoded->gainMap->image->height, image->gainMap->image->height);
  EXPECT_EQ(decoded->gainMap->image->depth, image->gainMap->image->depth);
  EXPECT_EQ(decoded->gainMap->image->colorPrimaries,
            image->gainMap->image->colorPrimaries);
  EXPECT_EQ(decoded->gainMap->image->transferCharacteristics,
            image->gainMap->image->transferCharacteristics);
  EXPECT_EQ(decoded->gainMap->image->matrixCoefficients,
            image->gainMap->image->matrixCoefficients);
  EXPECT_EQ(decoded->gainMap->image->yuvRange, image->gainMap->image->yuvRange);
  CheckGainMapMetadataMatches(*decoded->gainMap, *image->gainMap);

  ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);
  EXPECT_GT(testutil::GetPsnr(*image, *decoded, false), 40.0);
  EXPECT_GT(testutil::GetPsnr(*image->gainMap->image, *decoded->gainMap->image,
                              false),
            40.0);
}

TEST(GainMapTest, EncodeDecodeBaseImageHdr) {
  ImagePtr image = CreateTestImageWithGainMap(/*base_rendition_is_hdr=*/true);
  ASSERT_NE(image, nullptr);

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  AvifRwData encoded;
  ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded),
            AVIF_RESULT_OK);

  auto decoder = CreateDecoder(encoded);
  ASSERT_NE(decoder, nullptr);
  decoder->imageContentToDecode |= AVIF_IMAGE_CONTENT_GAIN_MAP;
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);

  const auto* decoded = decoder->image;
  EXPECT_GT(testutil::GetPsnr(*image, *decoded, false), 40.0);
  ASSERT_NE(decoded->gainMap, nullptr);
  ASSERT_NE(decoded->gainMap->image, nullptr);
  EXPECT_GT(testutil::GetPsnr(*image->gainMap->image, *decoded->gainMap->image,
                              false),
            40.0);
  EXPECT_EQ(decoded->clli.maxCLL, image->clli.maxCLL);
  EXPECT_EQ(decoded->clli.maxPALL, image->clli.maxPALL);
  EXPECT_EQ(decoded->gainMap->altCLLI.maxCLL, 0u);
  EXPECT_EQ(decoded->gainMap->altCLLI.maxPALL, 0u);
  EXPECT_EQ(decoded->gainMap->altDepth, 8u);
  EXPECT_EQ(decoded->gainMap->altPlaneCount, 3u);
  EXPECT_EQ(decoded->gainMap->altColorPrimaries, AVIF_COLOR_PRIMARIES_BT601);
  EXPECT_EQ(decoded->gainMap->altTransferCharacteristics,
            AVIF_TRANSFER_CHARACTERISTICS_SRGB);
  EXPECT_EQ(decoded->gainMap->altMatrixCoefficients,
            AVIF_MATRIX_COEFFICIENTS_SMPTE2085);
  EXPECT_EQ(decoded->gainMap->image->width, image->gainMap->image->width);
  EXPECT_EQ(decoded->gainMap->image->height, image->gainMap->image->height);
  EXPECT_EQ(decoded->gainMap->image->depth, image->gainMap->image->depth);
  CheckGainMapMetadataMatches(*decoded->gainMap, *image->gainMap);
}

TEST(GainMapTest, EncodeDecodeOrientedNotEqual) {
  ImagePtr image = CreateTestImageWithGainMap(/*base_rendition_is_hdr=*/false);
  ASSERT_NE(image, nullptr);
  image->gainMap->image->transformFlags = AVIF_TRANSFORM_IMIR;
  // The gain map should have no transformative property. Expect a failure.
  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  AvifRwData encoded;
  ASSERT_NE(avifEncoderWrite(encoder.get(), image.get(), &encoded),
            AVIF_RESULT_OK);
}

TEST(GainMapTest, EncodeDecodeOriented) {
  ImagePtr image = CreateTestImageWithGainMap(/*base_rendition_is_hdr=*/false);
  ASSERT_NE(image, nullptr);
  image->transformFlags = AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR;
  image->irot.angle = 1;
  image->imir.axis = 0;

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  AvifRwData encoded;
  ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded),
            AVIF_RESULT_OK);

  auto decoder = CreateDecoder(encoded);
  ASSERT_NE(decoder, nullptr);
  decoder->imageContentToDecode |= AVIF_IMAGE_CONTENT_GAIN_MAP;
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);

  EXPECT_EQ(decoder->image->transformFlags, image->transformFlags);
  EXPECT_EQ(decoder->image->irot.angle, image->irot.angle);
  EXPECT_EQ(decoder->image->imir.axis, image->imir.axis);
  EXPECT_EQ(decoder->image->gainMap->image->transformFlags,
            AVIF_TRANSFORM_NONE);
}

TEST(GainMapTest, EncodeDecodeMetadataSameDenominator) {
  ImagePtr image = CreateTestImageWithGainMap(/*base_rendition_is_hdr=*/true);
  ASSERT_NE(image, nullptr);

  const uint32_t kDenominator = 1000;
  image->gainMap->baseHdrHeadroom.d = kDenominator;
  image->gainMap->alternateHdrHeadroom.d = kDenominator;
  for (int c = 0; c < 3; ++c) {
    image->gainMap->baseOffset[c].d = kDenominator;
    image->gainMap->alternateOffset[c].d = kDenominator;
    image->gainMap->gainMapGamma[c].d = kDenominator;
    image->gainMap->gainMapMin[c].d = kDenominator;
    image->gainMap->gainMapMax[c].d = kDenominator;
  }

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  AvifRwData encoded;
  ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded),
            AVIF_RESULT_OK);

  auto decoder = CreateDecoder(encoded);
  ASSERT_NE(decoder, nullptr);
  decoder->imageContentToDecode |= AVIF_IMAGE_CONTENT_GAIN_MAP;
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);

  ASSERT_NE(decoder->image->gainMap, nullptr);
  CheckGainMapMetadataMatches(*decoder->image->gainMap, *image->gainMap);
}

TEST(GainMapTest, EncodeDecodeMetadataAllChannelsIdentical) {
  ImagePtr image = CreateTestImageWithGainMap(/*base_rendition_is_hdr=*/true);
  ASSERT_NE(image, nullptr);

  for (int c = 0; c < 3; ++c) {
    image->gainMap->baseOffset[c] = {1, 2};
    image->gainMap->alternateOffset[c] = {3, 4};
    image->gainMap->gainMapGamma[c] = {5, 6};
    image->gainMap->gainMapMin[c] = {7, 8};
    image->gainMap->gainMapMax[c] = {9, 10};
  }

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  AvifRwData encoded;
  ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded),
            AVIF_RESULT_OK);

  auto decoder = CreateDecoder(encoded);
  ASSERT_NE(decoder, nullptr);
  decoder->imageContentToDecode |= AVIF_IMAGE_CONTENT_GAIN_MAP;
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);

  ASSERT_NE(decoder->image->gainMap, nullptr);
  CheckGainMapMetadataMatches(*decoder->image->gainMap, *image->gainMap);
}

TEST(GainMapTest, EncodeDecodeGrid) {
  std::vector<ImagePtr> cells;
  std::vector<const avifImage*> cell_ptrs;
  std::vector<const avifImage*> gain_map_ptrs;
  constexpr int kGridCols = 2;
  constexpr int kGridRows = 2;
  constexpr int kCellWidth = 128;
  constexpr int kCellHeight = 200;

  for (int i = 0; i < kGridCols * kGridRows; ++i) {
    const int gradient_offset = i * 10;
    ImagePtr image = testutil::CreateImage(
        kCellWidth, kCellHeight, /*depth=*/10, AVIF_PIXEL_FORMAT_YUV444,
        AVIF_PLANES_ALL, AVIF_RANGE_FULL);
    ASSERT_NE(image, nullptr);
    image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_PQ;
    testutil::FillImageGradient(image.get(), gradient_offset);
    ImagePtr gain_map = testutil::CreateImage(
        kCellWidth / 2, kCellHeight / 2, /*depth=*/8, AVIF_PIXEL_FORMAT_YUV420,
        AVIF_PLANES_YUV, AVIF_RANGE_FULL);
    ASSERT_NE(gain_map, nullptr);
    testutil::FillImageGradient(gain_map.get(), gradient_offset);
    image->gainMap = avifGainMapCreate();
    ASSERT_NE(image->gainMap, nullptr);
    image->gainMap->image = gain_map.release();
    FillTestGainMapMetadata(/*base_rendition_is_hdr=*/true, image->gainMap);

    cell_ptrs.push_back(image.get());
    gain_map_ptrs.push_back(image->gainMap->image);
    cells.push_back(std::move(image));
  }

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  AvifRwData encoded;
  ASSERT_EQ(
      avifEncoderAddImageGrid(encoder.get(), kGridCols, kGridRows,
                              cell_ptrs.data(), AVIF_ADD_IMAGE_FLAG_SINGLE),
      AVIF_RESULT_OK);
  ASSERT_EQ(avifEncoderFinish(encoder.get(), &encoded), AVIF_RESULT_OK);

  auto decoder = CreateDecoder(encoded);
  ASSERT_NE(decoder, nullptr);
  decoder->imageContentToDecode |= AVIF_IMAGE_CONTENT_GAIN_MAP;
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);
  const auto* decoded = decoder->image;
  ASSERT_NE(decoded, nullptr);

  ImagePtr merged = testutil::CreateImage(
      static_cast<int>(decoded->width), static_cast<int>(decoded->height),
      decoded->depth, decoded->yuvFormat, AVIF_PLANES_ALL, AVIF_RANGE_FULL);
  ASSERT_EQ(testutil::MergeGridFromRawPointers(kGridCols, kGridRows, cell_ptrs,
                                               merged.get()),
            AVIF_RESULT_OK);
  ASSERT_GT(testutil::GetPsnr(*merged, *decoded, false), 40.0);

  ASSERT_NE(decoded->gainMap, nullptr);
  ASSERT_NE(decoded->gainMap->image, nullptr);

  ImagePtr merged_gain_map = testutil::CreateImage(
      static_cast<int>(decoded->gainMap->image->width),
      static_cast<int>(decoded->gainMap->image->height),
      decoded->gainMap->image->depth, decoded->gainMap->image->yuvFormat,
      AVIF_PLANES_YUV, AVIF_RANGE_FULL);
  ASSERT_EQ(testutil::MergeGridFromRawPointers(
                kGridCols, kGridRows, gain_map_ptrs, merged_gain_map.get()),
            AVIF_RESULT_OK);
  ASSERT_GT(
      testutil::GetPsnr(*merged_gain_map, *decoded->gainMap->image, false),
      40.0);
  CheckGainMapMetadataMatches(*decoded->gainMap, *cell_ptrs[0]->gainMap);
}

TEST(GainMapTest, InvalidGrid) {
  std::vector<ImagePtr> cells;
  std::vector<const avifImage*> cell_ptrs;
  constexpr int kGridCols = 2;
  constexpr int kGridRows = 2;

  for (int i = 0; i < kGridCols * kGridRows; ++i) {
    ImagePtr image = testutil::CreateImage(
        /*width=*/64, /*height=*/100, /*depth=*/10, AVIF_PIXEL_FORMAT_YUV444,
        AVIF_PLANES_ALL, AVIF_RANGE_FULL);
    ASSERT_NE(image, nullptr);
    image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_PQ;
    testutil::FillImageGradient(image.get(), 0);
    ImagePtr gain_map = testutil::CreateImage(
        /*width=*/64, /*height=*/100, /*depth=*/8, AVIF_PIXEL_FORMAT_YUV420,
        AVIF_PLANES_YUV, AVIF_RANGE_FULL);
    ASSERT_NE(gain_map, nullptr);
    testutil::FillImageGradient(gain_map.get(), 0);
    image->gainMap = avifGainMapCreate();
    ASSERT_NE(image->gainMap, nullptr);
    image->gainMap->image = gain_map.release();
    FillTestGainMapMetadata(/*base_rendition_is_hdr=*/true, image->gainMap);

    cell_ptrs.push_back(image.get());
    cells.push_back(std::move(image));
  }

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  AvifRwData encoded;

  // Invalid: one cell has the wrong size.
  cells[1]->gainMap->image->height = 90;
  ASSERT_NE(
      avifEncoderAddImageGrid(encoder.get(), kGridCols, kGridRows,
                              cell_ptrs.data(), AVIF_ADD_IMAGE_FLAG_SINGLE),
      AVIF_RESULT_OK);
  cells[1]->gainMap->image->height =
      cells[0]->gainMap->image->height;  // Revert.

  // Invalid: one cell has a different depth.
  cells[1]->gainMap->image->depth = 12;
  ASSERT_NE(
      avifEncoderAddImageGrid(encoder.get(), kGridCols, kGridRows,
                              cell_ptrs.data(), AVIF_ADD_IMAGE_FLAG_SINGLE),
      AVIF_RESULT_OK);
  cells[1]->gainMap->image->depth = cells[0]->gainMap->image->depth;  // Revert.

  // Invalid: one cell has different gain map metadata
  cells[1]->gainMap->gainMapGamma[0].n = 42;
  ASSERT_NE(
      avifEncoderAddImageGrid(encoder.get(), kGridCols, kGridRows,
                              cell_ptrs.data(), AVIF_ADD_IMAGE_FLAG_SINGLE),
      AVIF_RESULT_OK);
  cells[1]->gainMap->gainMapGamma[0].n =
      cells[0]->gainMap->gainMapGamma[0].n;  // Revert.
}

TEST(GainMapTest, NoGainMap) {
  ImagePtr image = testutil::CreateImage(/*width=*/12, /*height=*/34,
                                         /*depth=*/10, AVIF_PIXEL_FORMAT_YUV420,
                                         AVIF_PLANES_ALL, AVIF_RANGE_FULL);
  ASSERT_NE(image, nullptr);
  image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_SRGB;
  testutil::FillImageGradient(image.get(), 0);
  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  AvifRwData encoded;
  ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded),
            AVIF_RESULT_OK);

  auto decoder = CreateDecoder(encoded);
  ASSERT_NE(decoder, nullptr);
  decoder->imageContentToDecode |= AVIF_IMAGE_CONTENT_GAIN_MAP;
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);
  const auto* decoded = decoder->image;
  ASSERT_NE(decoded, nullptr);

  EXPECT_GT(testutil::GetPsnr(*image, *decoded, false), 40.0);
  EXPECT_EQ(decoded->gainMap, nullptr);
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
