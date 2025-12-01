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
#include <cstdint>
#include <iostream>

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
