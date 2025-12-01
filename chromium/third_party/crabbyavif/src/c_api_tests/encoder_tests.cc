/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <tuple>
#include <vector>

#include "avif/avif.h"
#include "gtest/gtest.h"
#include "testutil.h"

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

// ICC color profiles are not checked by crabbyavif so the content does not
// matter. This is a truncated widespread ICC color profile.
constexpr std::array<uint8_t, 24> kSampleIcc = {
    0x00, 0x00, 0x02, 0x0c, 0x6c, 0x63, 0x6d, 0x73, 0x02, 0x10, 0x00, 0x00,
    0x6d, 0x6e, 0x74, 0x72, 0x52, 0x47, 0x42, 0x20, 0x58, 0x59, 0x5a, 0x20};

// XMP bytes are not checked by crabbyavif so the content does not matter. This
// is a truncated widespread XMP metadata chunk.
constexpr std::array<uint8_t, 24> kSampleXmp = {
    0x3c, 0x3f, 0x78, 0x70, 0x61, 0x63, 0x6b, 0x65, 0x74, 0x20, 0x62, 0x65,
    0x67, 0x69, 0x6e, 0x3d, 0x22, 0xef, 0xbb, 0xbf, 0x22, 0x20, 0x69, 0x64};

// Exif bytes are partially checked by crabbyavif. This is a truncated
// widespread Exif metadata chunk.
constexpr std::array<uint8_t, 24> kSampleExif = {
    0xff, 0x1,  0x45, 0x78, 0x69, 0x76, 0x32, 0xff, 0xe1, 0x12, 0x5a, 0x45,
    0x78, 0x69, 0x66, 0x0,  0x0,  0x49, 0x49, 0x2a, 0x0,  0x8,  0x0,  0x0};

DecoderPtr CreateDecoder(const AvifRwData& encoded) {
  DecoderPtr decoder(avifDecoderCreate());
  if (decoder == nullptr ||
      avifDecoderSetIOMemory(decoder.get(), encoded.data, encoded.size) !=
          AVIF_RESULT_OK) {
    return nullptr;
  }
  return decoder;
}

TEST(BasicTest, EncodeDecode) {
  ImagePtr image = testutil::CreateImage(/*width=*/12, /*height=*/34,
                                         /*depth=*/8, AVIF_PIXEL_FORMAT_YUV420,
                                         AVIF_PLANES_ALL, AVIF_RANGE_FULL);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get(), /*offset=*/0);

  EncoderPtr encoder(avifEncoderCreate());
  encoder->quality = 70;
  encoder->speed = 10;
  ASSERT_NE(encoder, nullptr);
  AvifRwData encoded;
  ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded),
            AVIF_RESULT_OK);

  auto decoder = CreateDecoder(encoded);
  ASSERT_NE(decoder, nullptr);
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);
  EXPECT_EQ(decoder->image->width, image->width);
  EXPECT_EQ(decoder->image->height, image->height);
  EXPECT_EQ(decoder->image->depth, image->depth);
  ASSERT_GT(testutil::GetPsnr(*image, *decoder->image, /*ignore_alpha=*/false),
            40.0);
}

TEST(TransformTest, ClapIrotImir) {
  ImagePtr image = testutil::CreateImage(/*width=*/12, /*height=*/34,
                                         /*depth=*/8, AVIF_PIXEL_FORMAT_YUV444,
                                         AVIF_PLANES_ALL, AVIF_RANGE_FULL);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get(), /*offset=*/0);
  image->transformFlags |= AVIF_TRANSFORM_CLAP;
  avifDiagnostics diag{};
  const avifCropRect rect{/*x=*/4, /*y=*/6, /*width=*/8, /*height=*/10};
  ASSERT_TRUE(avifCleanApertureBoxConvertCropRect(&image->clap, &rect,
                                                  image->width, image->height,
                                                  image->yuvFormat, &diag));
  image->transformFlags |= AVIF_TRANSFORM_IROT;
  image->irot.angle = 1;
  image->transformFlags |= AVIF_TRANSFORM_IMIR;
  image->imir.axis = 1;

  EncoderPtr encoder(avifEncoderCreate());
  encoder->speed = 10;
  ASSERT_NE(encoder, nullptr);
  AvifRwData encoded;
  ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded),
            AVIF_RESULT_OK);

  auto decoder = CreateDecoder(encoded);
  ASSERT_NE(decoder, nullptr);
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);

  EXPECT_EQ(decoder->image->transformFlags, image->transformFlags);
  EXPECT_EQ(decoder->image->clap.widthN, image->clap.widthN);
  EXPECT_EQ(decoder->image->clap.widthD, image->clap.widthD);
  EXPECT_EQ(decoder->image->clap.heightN, image->clap.heightN);
  EXPECT_EQ(decoder->image->clap.heightD, image->clap.heightD);
  EXPECT_EQ(decoder->image->clap.horizOffN, image->clap.horizOffN);
  EXPECT_EQ(decoder->image->clap.horizOffD, image->clap.horizOffD);
  EXPECT_EQ(decoder->image->clap.vertOffN, image->clap.vertOffN);
  EXPECT_EQ(decoder->image->clap.vertOffD, image->clap.vertOffD);
  EXPECT_EQ(decoder->image->irot.angle, image->irot.angle);
  EXPECT_EQ(decoder->image->imir.axis, image->imir.axis);
}

TEST(MetadataTest, IccExifXmp) {
  ImagePtr image = testutil::CreateImage(/*width=*/12, /*height=*/34,
                                         /*depth=*/8, AVIF_PIXEL_FORMAT_YUV444,
                                         AVIF_PLANES_ALL, AVIF_RANGE_FULL);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get(), /*offset=*/0);
  ASSERT_EQ(avifRWDataSet(&image->icc, kSampleIcc.data(), kSampleIcc.size()),
            AVIF_RESULT_OK);
  ASSERT_EQ(avifRWDataSet(&image->exif, kSampleExif.data(), kSampleExif.size()),
            AVIF_RESULT_OK);
  ASSERT_EQ(avifRWDataSet(&image->xmp, kSampleXmp.data(), kSampleXmp.size()),
            AVIF_RESULT_OK);

  EncoderPtr encoder(avifEncoderCreate());
  encoder->speed = 10;
  ASSERT_NE(encoder, nullptr);
  AvifRwData encoded;
  ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded),
            AVIF_RESULT_OK);

  auto decoder = CreateDecoder(encoded);
  ASSERT_NE(decoder, nullptr);
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);

  EXPECT_TRUE(testutil::AreByteSequencesEqual(
      decoder->image->icc.data, decoder->image->icc.size, image->icc.data,
      image->icc.size));
  EXPECT_TRUE(testutil::AreByteSequencesEqual(
      decoder->image->exif.data, decoder->image->exif.size, image->exif.data,
      image->exif.size));
  EXPECT_TRUE(testutil::AreByteSequencesEqual(
      decoder->image->xmp.data, decoder->image->xmp.size, image->xmp.data,
      image->xmp.size));
}

class LosslessRoundTrip
    : public testing::TestWithParam<
          std::tuple<avifMatrixCoefficients, avifPixelFormat>> {};

TEST_P(LosslessRoundTrip, RoundTrip) {
  const auto matrix_coefficients = std::get<0>(GetParam());
  const auto pixel_format = std::get<1>(GetParam());

  ImagePtr image = testutil::CreateImage(/*width=*/12, /*height=*/34,
                                         /*depth=*/8, pixel_format,
                                         AVIF_PLANES_ALL, AVIF_RANGE_FULL);
  ASSERT_NE(image, nullptr);
  image->matrixCoefficients = matrix_coefficients;
  testutil::FillImageGradient(image.get(), /*offset=*/0);

  // Encode.
  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  encoder->speed = 10;
  encoder->quality = 100;
  AvifRwData encoded;
  avifResult result = avifEncoderWrite(encoder.get(), image.get(), &encoded);

  if (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY &&
      image->yuvFormat != AVIF_PIXEL_FORMAT_YUV444) {
    // The AV1 spec does not allow identity with subsampling.
    ASSERT_NE(result, AVIF_RESULT_OK);
    return;
  }
  ASSERT_EQ(result, AVIF_RESULT_OK);

  // Decode.
  auto decoder = CreateDecoder(encoded);
  ASSERT_NE(decoder, nullptr);
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);

  ASSERT_TRUE(testutil::AreImagesEqual(*image, *decoder->image,
                                       /*ignore_alpha=*/false));
}

INSTANTIATE_TEST_SUITE_P(
    LosslessRoundTripTests, LosslessRoundTrip,
    testing::Combine(testing::Values(AVIF_MATRIX_COEFFICIENTS_IDENTITY,
                                     AVIF_MATRIX_COEFFICIENTS_YCGCO,
                                     AVIF_MATRIX_COEFFICIENTS_YCGCO_RE),
                     testing::Values(AVIF_PIXEL_FORMAT_YUV444,
                                     AVIF_PIXEL_FORMAT_YUV420,
                                     AVIF_PIXEL_FORMAT_YUV400)));

struct Cell {
  int width;
  int height;
};

avifResult EncodeDecodeGrid(const std::vector<std::vector<Cell>>& cell_rows,
                            avifPixelFormat yuv_format) {
  // Construct a grid.
  std::vector<ImagePtr> cell_images;
  cell_images.reserve(cell_rows.size() * cell_rows.front().size());
  for (const auto& cell_row : cell_rows) {
    for (const auto& cell : cell_row) {
      cell_images.emplace_back(
          testutil::CreateImage(cell.width, cell.height, /*depth=*/8,
                                yuv_format, AVIF_PLANES_ALL, AVIF_RANGE_FULL));
      if (!cell_images.back()) {
        return AVIF_RESULT_INVALID_ARGUMENT;
      }
      testutil::FillImageGradient(cell_images.back().get(), 0);
    }
  }

  // Encode the grid image (losslessly for easy pixel-by-pixel comparison).
  EncoderPtr encoder(avifEncoderCreate());
  if (!encoder) {
    return AVIF_RESULT_OUT_OF_MEMORY;
  }
  encoder->speed = 10;
  encoder->quality = 100;
  encoder->qualityAlpha = 100;
  std::vector<avifImage*> cell_image_ptrs(cell_images.size());
  for (size_t i = 0; i < cell_images.size(); ++i) {
    cell_image_ptrs[i] = cell_images[i].get();
  }
  avifResult result = avifEncoderAddImageGrid(
      encoder.get(), static_cast<uint32_t>(cell_rows.front().size()),
      static_cast<uint32_t>(cell_rows.size()), cell_image_ptrs.data(),
      AVIF_ADD_IMAGE_FLAG_SINGLE);
  if (result != AVIF_RESULT_OK) {
    return result;
  }

  AvifRwData encoded_avif;
  result = avifEncoderFinish(encoder.get(), &encoded_avif);
  if (result != AVIF_RESULT_OK) {
    return result;
  }

  // Decode the grid image.
  ImagePtr image(avifImageCreateEmpty());
  DecoderPtr decoder(avifDecoderCreate());
  if (!image || !decoder) {
    return AVIF_RESULT_OUT_OF_MEMORY;
  }
  result = avifDecoderReadMemory(decoder.get(), image.get(), encoded_avif.data,
                                 encoded_avif.size);
  if (result != AVIF_RESULT_OK) {
    return result;
  }

  // Reconstruct the input image by merging all cells into a single avifImage.
  ImagePtr grid = testutil::CreateImage(
      static_cast<int>(image->width), static_cast<int>(image->height),
      /*depth=*/8, yuv_format, AVIF_PLANES_ALL, AVIF_RANGE_FULL);
  const int num_rows = (int)cell_rows.size();
  const int num_cols = (int)cell_rows[0].size();
  AVIF_CHECKRES(
      testutil::MergeGrid(num_cols, num_rows, cell_images, grid.get()));

  if ((grid->width != image->width) || (grid->height != image->height) ||
      !testutil::AreImagesEqual(*image, *grid, false)) {
    return AVIF_RESULT_UNKNOWN_ERROR;
  }

  return AVIF_RESULT_OK;
}

struct GridTestParam {
  std::vector<std::vector<Cell>> cells;
  avifResult expected_result;
};

class GridApiTest : public testing::TestWithParam<
                        std::tuple<GridTestParam, avifPixelFormat>> {};

TEST_P(GridApiTest, EncodeDecodeGrid) {
  const auto& [test_param, pixel_format] = GetParam();
  EXPECT_EQ(EncodeDecodeGrid(test_param.cells, pixel_format),
            test_param.expected_result);
}

INSTANTIATE_TEST_SUITE_P(
    AllGridTests, GridApiTest,
    testing::Combine(
        testing::Values(
            // Single cells.
            GridTestParam{{{{1, 1}}}, AVIF_RESULT_OK},
            GridTestParam{{{{1, 64}}}, AVIF_RESULT_OK},
            GridTestParam{{{{64, 1}}}, AVIF_RESULT_OK},
            GridTestParam{{{{64, 64}}}, AVIF_RESULT_OK},
            GridTestParam{{{{127, 127}}}, AVIF_RESULT_OK},
            // Cells of same dimension.
            GridTestParam{{{{64, 64}, {64, 64}, {64, 64}}}, AVIF_RESULT_OK},
            GridTestParam{{{{100, 110}},  //
                           {{100, 110}},  //
                           {{100, 110}}},
                          AVIF_RESULT_OK},
            GridTestParam{{{{64, 64}, {64, 64}, {64, 64}},
                           {{64, 64}, {64, 64}, {64, 64}},
                           {{64, 64}, {64, 64}, {64, 64}}},
                          AVIF_RESULT_OK},
            // Cells are too small.
            GridTestParam{{{{2, 64}, {2, 64}}}, AVIF_RESULT_INVALID_IMAGE_GRID},
            GridTestParam{{{{64, 62}, {64, 62}}},
                          AVIF_RESULT_INVALID_IMAGE_GRID},
            GridTestParam{{{{64, 2}},  //
                           {{64, 2}}},
                          AVIF_RESULT_INVALID_IMAGE_GRID},
            GridTestParam{{{{2, 64}},  //
                           {{2, 64}}},
                          AVIF_RESULT_INVALID_IMAGE_GRID},
            // Right-most cells are narrower.
            GridTestParam{{{{100, 100}, {100, 100}, {66, 100}}},
                          AVIF_RESULT_OK},
            // Bottom-most cells are shorter.
            GridTestParam{{{{100, 100}, {100, 100}},
                           {{100, 100}, {100, 100}},
                           {{100, 66}, {100, 66}}},
                          AVIF_RESULT_OK},
            // Right-most cells are narrower and bottom-most cells are shorter.
            GridTestParam{{{{100, 100}, {100, 100}, {66, 100}},
                           {{100, 100}, {100, 100}, {66, 100}},
                           {{100, 66}, {100, 66}, {66, 66}}},
                          AVIF_RESULT_OK},
            // Right-most cells are wider.
            GridTestParam{{{{100, 100}, {100, 100}, {222, 100}},
                           {{100, 100}, {100, 100}, {222, 100}},
                           {{100, 100}, {100, 100}, {222, 100}}},
                          AVIF_RESULT_INVALID_IMAGE_GRID},
            // Bottom-most cells are taller.
            GridTestParam{{{{100, 100}, {100, 100}, {100, 100}},
                           {{100, 100}, {100, 100}, {100, 100}},
                           {{100, 222}, {100, 222}, {100, 222}}},
                          AVIF_RESULT_INVALID_IMAGE_GRID},
            // One cell dimension is off.
            GridTestParam{{{{100, 100}, {100, 100}, {100, 100}},
                           {{100, 100}, {66, 100}, {100, 100}},
                           {{100, 100}, {100, 100}, {100, 100}}},
                          AVIF_RESULT_INVALID_IMAGE_GRID},
            GridTestParam{{{{100, 100}, {100, 100}, {66, 100}},
                           {{100, 100}, {100, 100}, {66, 100}},
                           {{100, 66}, {100, 66}, {66, 100}}},
                          AVIF_RESULT_INVALID_IMAGE_GRID}),
        testing::Values(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
                        AVIF_PIXEL_FORMAT_YUV420, AVIF_PIXEL_FORMAT_YUV400)));

TEST(GridApiTest, OddDimensionsWithSubsampledYuvFormat) {
  // ISO/IEC 23000-22:2019, Section 7.3.11.4.2:
  //   - when the images are in the 4:2:2 chroma sampling format the horizontal
  //     tile offsets and widths, and the output width, shall be even numbers;
  EXPECT_EQ(EncodeDecodeGrid({{{64, 65}, {64, 65}}}, AVIF_PIXEL_FORMAT_YUV422),
            AVIF_RESULT_OK);
  EXPECT_NE(EncodeDecodeGrid({{{65, 64}, {65, 64}}}, AVIF_PIXEL_FORMAT_YUV422),
            AVIF_RESULT_OK);
  //   - when the images are in the 4:2:0 chroma sampling format both the
  //     horizontal and vertical tile offsets and widths, and the output width
  //     and height, shall be even numbers.
  EXPECT_NE(EncodeDecodeGrid({{{64, 65}, {64, 65}}}, AVIF_PIXEL_FORMAT_YUV420),
            AVIF_RESULT_OK);
  EXPECT_NE(EncodeDecodeGrid({{{65, 64}, {65, 64}}}, AVIF_PIXEL_FORMAT_YUV420),
            AVIF_RESULT_OK);
  // ISO/IEC 23000-22:2019, Section 7.3.11.4.2:
  //   - when the images are in the 4:2:2 chroma sampling format the horizontal
  //     tile offsets and widths, and the output width, shall be even numbers;
  EXPECT_EQ(EncodeDecodeGrid({{{66, 66}},  //
                              {{66, 65}}},
                             AVIF_PIXEL_FORMAT_YUV422),
            AVIF_RESULT_OK);
  EXPECT_NE(EncodeDecodeGrid({{{66, 66}, {65, 66}}}, AVIF_PIXEL_FORMAT_YUV422),
            AVIF_RESULT_OK);
  //   - when the images are in the 4:2:0 chroma sampling format both the
  //     horizontal and vertical tile offsets and widths, and the output width
  //     and height, shall be even numbers.
  EXPECT_NE(EncodeDecodeGrid({{{66, 66}},  //
                              {{66, 65}}},
                             AVIF_PIXEL_FORMAT_YUV420),
            AVIF_RESULT_OK);
  EXPECT_NE(EncodeDecodeGrid({{{66, 66}, {65, 66}}}, AVIF_PIXEL_FORMAT_YUV420),
            AVIF_RESULT_OK);
}

TEST(GridApiTest, MatrixCoefficients) {
  for (const auto same_matrix_coefficients : {true, false}) {
    ImagePtr cell_0 =
        testutil::CreateImage(64, 64, /*depth=*/8, AVIF_PIXEL_FORMAT_YUV444,
                              AVIF_PLANES_ALL, AVIF_RANGE_FULL);
    ImagePtr cell_1 =
        testutil::CreateImage(1, 64, /*depth=*/8, AVIF_PIXEL_FORMAT_YUV444,
                              AVIF_PLANES_ALL, AVIF_RANGE_FULL);
    ASSERT_NE(cell_0, nullptr);
    ASSERT_NE(cell_1, nullptr);

    testutil::FillImageGradient(cell_0.get(), 0);
    testutil::FillImageGradient(cell_1.get(), 0);

    cell_0->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT601;
    if (same_matrix_coefficients) {
      cell_1->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT601;
    } else {
      cell_1->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_UNSPECIFIED;
    }

    EncoderPtr encoder(avifEncoderCreate());
    ASSERT_NE(encoder, nullptr);
    encoder->speed = 10;
    const avifImage* cell_image_ptrs[2] = {cell_0.get(), cell_1.get()};
    const auto res =
        avifEncoderAddImageGrid(encoder.get(), /*gridCols=*/2, /*gridRows=*/1,
                                cell_image_ptrs, AVIF_ADD_IMAGE_FLAG_SINGLE);
    if (same_matrix_coefficients) {
      ASSERT_EQ(res, AVIF_RESULT_OK);
      AvifRwData encoded;
      ASSERT_EQ(avifEncoderFinish(encoder.get(), &encoded), AVIF_RESULT_OK);
      auto decoder = CreateDecoder(encoded);
      ASSERT_NE(decoder, nullptr);
      ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
      ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);
    } else {
      ASSERT_NE(res, AVIF_RESULT_OK);
    }
  }
}

class ProgressiveTest : public testing::Test {
 protected:
  static constexpr uint32_t kImageSize = 256;

  void SetUp() override {
    ASSERT_NE(encoder_, nullptr);
    encoder_->codecChoice = AVIF_CODEC_CHOICE_AOM;
    encoder_->speed = 10;

    ASSERT_NE(decoder_, nullptr);
    decoder_->allowProgressive = true;

    ASSERT_NE(image_, nullptr);
    testutil::FillImageGradient(image_.get(), 0);
  }

  void TestDecode(uint32_t expect_width, uint32_t expect_height, bool is_grid,
                  bool check_psnr) {
    ASSERT_EQ(avifDecoderSetIOMemory(decoder_.get(), encoded_avif_.data,
                                     encoded_avif_.size),
              AVIF_RESULT_OK);
    ASSERT_EQ(avifDecoderParse(decoder_.get()), AVIF_RESULT_OK);
    EXPECT_EQ(decoder_->progressiveState, AVIF_PROGRESSIVE_STATE_ACTIVE);
    EXPECT_EQ(static_cast<uint32_t>(decoder_->imageCount),
              encoder_->extraLayerCount + 1);

    ImagePtr grid;
    if (is_grid) {
      grid = testutil::CreateImage(
          static_cast<int>(expect_width), static_cast<int>(expect_height),
          image_->depth, image_->yuvFormat, AVIF_PLANES_ALL, image_->yuvRange);
      ASSERT_EQ(testutil::MergeGridFromRawPointers(
                    2, 1, {image_.get(), image_.get()}, grid.get()),
                AVIF_RESULT_OK);
    };

    std::vector<double> psnr;
    for (uint32_t layer = 0; layer < encoder_->extraLayerCount + 1; ++layer) {
      ASSERT_EQ(avifDecoderNextImage(decoder_.get()), AVIF_RESULT_OK);
      EXPECT_EQ(decoder_->image->width, expect_width);
      EXPECT_EQ(decoder_->image->height, expect_height);
      psnr.push_back(testutil::GetPsnr(is_grid ? *grid : *image_,
                                       *decoder_->image,
                                       /*ignore_alpha=*/false));
    }
    if (check_psnr) {
      // Ensure that psnr of layers is in non-decreasing order. All the tests
      // encode layers in non-decreasing order of quality.
      for (size_t i = 1; i < psnr.size(); ++i) {
        ASSERT_GE(psnr[i], psnr[i - 1]);
      }
    }
  }

  EncoderPtr encoder_{avifEncoderCreate()};
  DecoderPtr decoder_{avifDecoderCreate()};

  ImagePtr image_ =
      testutil::CreateImage(kImageSize, kImageSize, 8, AVIF_PIXEL_FORMAT_YUV444,
                            AVIF_PLANES_YUV, AVIF_RANGE_FULL);

  AvifRwData encoded_avif_;
};

TEST_F(ProgressiveTest, QualityChange) {
  encoder_->extraLayerCount = 1;
  encoder_->quality = 2;

  ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  encoder_->quality = 80;
  ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  ASSERT_EQ(avifEncoderFinish(encoder_.get(), &encoded_avif_), AVIF_RESULT_OK);

  TestDecode(kImageSize, kImageSize, /*is_grid=*/false, /*check_psnr=*/true);
}

TEST_F(ProgressiveTest, DimensionChange) {
  encoder_->extraLayerCount = 1;
  encoder_->quality = 80;
  encoder_->scalingMode = {{1, 2}, {1, 2}};

  ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  encoder_->scalingMode = {{1, 1}, {1, 1}};
  ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  ASSERT_EQ(avifEncoderFinish(encoder_.get(), &encoded_avif_), AVIF_RESULT_OK);

  // The first layer is scaled to a different dimension internally. So checking
  // for psnr against the original image is not useful.
  TestDecode(kImageSize, kImageSize, /*is_grid=*/false, /*check_psnr=*/false);
}

TEST_F(ProgressiveTest, LayeredGrid) {
  encoder_->extraLayerCount = 1;
  encoder_->quality = 2;

  avifImage* image_grid[2] = {image_.get(), image_.get()};
  ASSERT_EQ(avifEncoderAddImageGrid(encoder_.get(), 2, 1, image_grid,
                                    AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  encoder_->quality = 100;
  ASSERT_EQ(avifEncoderAddImageGrid(encoder_.get(), 2, 1, image_grid,
                                    AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  ASSERT_EQ(avifEncoderFinish(encoder_.get(), &encoded_avif_), AVIF_RESULT_OK);

  TestDecode(2 * kImageSize, kImageSize, /*is_grid=*/true, /*check_psnr=*/true);
}

TEST_F(ProgressiveTest, SameLayers) {
  encoder_->extraLayerCount = 3;
  for (uint32_t layer = 0; layer < encoder_->extraLayerCount + 1; ++layer) {
    ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                  AVIF_ADD_IMAGE_FLAG_NONE),
              AVIF_RESULT_OK);
  }
  ASSERT_EQ(avifEncoderFinish(encoder_.get(), &encoded_avif_), AVIF_RESULT_OK);

  TestDecode(kImageSize, kImageSize, /*is_grid=*/false, /*check_psnr=*/true);
}

TEST_F(ProgressiveTest, TooManyLayers) {
  encoder_->extraLayerCount = 1;

  ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);
  ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);
  ASSERT_NE(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);
}

TEST_F(ProgressiveTest, TooFewLayers) {
  encoder_->extraLayerCount = 1;

  ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  ASSERT_NE(avifEncoderFinish(encoder_.get(), &encoded_avif_), AVIF_RESULT_OK);
}

class SequenceApiTest
    : public testing::TestWithParam<
          std::tuple<uint32_t /*width*/, uint32_t /*height*/,
                     /*depth*/ int, avifPixelFormat, avifRange, bool /*alpha*/,
                     int /*repetitionCount*/>> {};

TEST_P(SequenceApiTest, EncodeDecodeSequence) {
  const auto width = std::get<0>(GetParam());
  const auto height = std::get<1>(GetParam());
  const auto depth = std::get<2>(GetParam());
  const auto yuvFormat = std::get<3>(GetParam());
  const auto yuvRange = std::get<4>(GetParam());
  const auto alpha = std::get<5>(GetParam());
  const auto repetition_count = std::get<6>(GetParam());

  ImagePtr image = testutil::CreateImage(
      width, height, depth, yuvFormat,
      alpha ? AVIF_PLANES_ALL : AVIF_PLANES_YUV, yuvRange);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get(), /*offset=*/0);

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  encoder->quality = 70;
  encoder->speed = 10;
  encoder->repetitionCount = repetition_count;

  static constexpr int kFrameCount = 10;
  static constexpr uint64_t kDurations[] = {1000, 2000,  1500, 1100, 2300,
                                            5000, 10000, 9000, 10,   500};
  static constexpr uint64_t kPts[] = {0,    1000,  3000,  4500,  5600,
                                      7900, 12900, 22900, 31900, 31910};
  for (const auto duration : kDurations) {
    ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), duration,
                                  AVIF_ADD_IMAGE_FLAG_NONE),
              AVIF_RESULT_OK);
  }
  AvifRwData encoded;
  ASSERT_EQ(avifEncoderFinish(encoder.get(), &encoded), AVIF_RESULT_OK);

  auto decoder = CreateDecoder(encoded);
  ASSERT_NE(decoder, nullptr);
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  EXPECT_EQ(decoder->image->width, image->width);
  EXPECT_EQ(decoder->image->height, image->height);
  EXPECT_EQ(decoder->image->depth, image->depth);
  EXPECT_TRUE(decoder->imageSequenceTrackPresent);
  EXPECT_EQ(decoder->repetitionCount,
            (repetition_count >= 0 && repetition_count < INT32_MAX)
                ? repetition_count
                : AVIF_REPETITION_COUNT_INFINITE);
  EXPECT_EQ(decoder->imageCount, kFrameCount);
  for (uint32_t i = 0; i < kFrameCount; ++i) {
    ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);
    ASSERT_GT(
        testutil::GetPsnr(*image, *decoder->image, /*ignore_alpha=*/false),
        40.0);
    avifImageTiming timing;
    ASSERT_EQ(avifDecoderNthImageTiming(decoder.get(), i, &timing),
              AVIF_RESULT_OK);
    EXPECT_EQ(timing.ptsInTimescales, kPts[i]);
    EXPECT_EQ(timing.durationInTimescales, kDurations[i]);
  }
}

INSTANTIATE_TEST_SUITE_P(
    AllSequenceTests, SequenceApiTest,
    testing::Combine(
        /*width=*/testing::Values(100, 121),
        /*height=*/testing::Values(200, 107),
        /*depth=*/testing::Values(8, 10, 12),
        testing::Values(AVIF_PIXEL_FORMAT_YUV420, AVIF_PIXEL_FORMAT_YUV422,
                        AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV400),
        testing::Values(AVIF_RANGE_LIMITED, AVIF_RANGE_FULL),
        /*alpha=*/testing::Bool(),
        /*repetitionCount=*/
        testing::Values(0, 5, INT32_MAX - 1, INT32_MAX, -20)));

TEST(EncoderApiTest, SetCodecSpecificOption) {
  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  EXPECT_NE(avifEncoderSetCodecSpecificOption(encoder.get(), nullptr, "abcd"),
            AVIF_RESULT_OK);
  EXPECT_NE(avifEncoderSetCodecSpecificOption(encoder.get(), "abcd", nullptr),
            AVIF_RESULT_OK);
  EXPECT_EQ(avifEncoderSetCodecSpecificOption(encoder.get(), "abcd", "efgh"),
            AVIF_RESULT_OK);
  EXPECT_EQ(
      avifEncoderSetCodecSpecificOption(encoder.get(), "color:abcd", "efgh"),
      AVIF_RESULT_OK);
  EXPECT_EQ(avifEncoderSetCodecSpecificOption(encoder.get(), "c:abcd", "efgh"),
            AVIF_RESULT_OK);
  EXPECT_EQ(avifEncoderSetCodecSpecificOption(encoder.get(), "g:abcd", "efgh"),
            AVIF_RESULT_OK);
  EXPECT_EQ(
      avifEncoderSetCodecSpecificOption(encoder.get(), "gainmap:abcd", "efgh"),
      AVIF_RESULT_OK);
  EXPECT_EQ(
      avifEncoderSetCodecSpecificOption(encoder.get(), "alpha:abcd", "efgh"),
      AVIF_RESULT_OK);
  EXPECT_EQ(avifEncoderSetCodecSpecificOption(encoder.get(), "a:abcd", "efgh"),
            AVIF_RESULT_OK);
}

TEST(PixelFormatToStringTest, Test) {
  EXPECT_STREQ(avifPixelFormatToString(AVIF_PIXEL_FORMAT_YUV444), "YUV444");
  EXPECT_STREQ(avifPixelFormatToString(AVIF_PIXEL_FORMAT_YUV422), "YUV422");
  EXPECT_STREQ(avifPixelFormatToString(AVIF_PIXEL_FORMAT_YUV420), "YUV420");
  EXPECT_STREQ(avifPixelFormatToString(AVIF_PIXEL_FORMAT_YUV400), "YUV400");
  EXPECT_STREQ(avifPixelFormatToString(AVIF_PIXEL_FORMAT_ANDROID_P010), "P010");
  EXPECT_STREQ(avifPixelFormatToString(AVIF_PIXEL_FORMAT_ANDROID_NV12), "NV12");
  EXPECT_STREQ(avifPixelFormatToString(AVIF_PIXEL_FORMAT_ANDROID_NV21), "NV21");
  EXPECT_STREQ(avifPixelFormatToString(AVIF_PIXEL_FORMAT_NONE), "Unknown");
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
