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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <tuple>
#include <vector>

#include "avif/avif.h"
#include "gtest/gtest.h"
#include "testutil.h"

namespace avif {
namespace {

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER) || defined(HWADDRESS_SANITIZER)
#define CRABBYAVIF_SANITIZER_BUILD
#endif

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

TEST(ImageTest, Create) {
  ImagePtr image(avifImageCreateEmpty());
  EXPECT_NE(image, nullptr);
  image.reset(avifImageCreate(0, 0, 0, AVIF_PIXEL_FORMAT_NONE));
  EXPECT_NE(image, nullptr);
  image.reset(avifImageCreate(1, 1, /*depth=*/1, AVIF_PIXEL_FORMAT_NONE));
  EXPECT_NE(image, nullptr);
  image.reset(avifImageCreate(64, 64, /*depth=*/8, AVIF_PIXEL_FORMAT_NONE));
  EXPECT_NE(image, nullptr);
  image.reset(avifImageCreate(std::numeric_limits<uint32_t>::max(),
                              std::numeric_limits<uint32_t>::max(),
                              /*depth=*/16, AVIF_PIXEL_FORMAT_NONE));
  EXPECT_NE(image, nullptr);
}

void TestAllocation(uint32_t width, uint32_t height, uint32_t depth,
                    avifPixelFormat yuv_format, avifPlanesFlags planes,
                    bool expect_success) {
  ImagePtr image(avifImageCreateEmpty());
  ASSERT_NE(image, nullptr);
  image->width = width;
  image->height = height;
  image->depth = depth;
  image->yuvFormat = yuv_format;
  auto res = avifImageAllocatePlanes(image.get(), planes);
  if (expect_success) {
    ASSERT_EQ(res, AVIF_RESULT_OK);
    if (yuv_format != AVIF_PIXEL_FORMAT_NONE && (planes & AVIF_PLANES_YUV)) {
      EXPECT_NE(image->yuvPlanes[AVIF_CHAN_Y], nullptr);
      if (yuv_format != AVIF_PIXEL_FORMAT_YUV400) {
        EXPECT_NE(image->yuvPlanes[AVIF_CHAN_U], nullptr);
        EXPECT_NE(image->yuvPlanes[AVIF_CHAN_V], nullptr);
      } else {
        EXPECT_EQ(image->yuvPlanes[AVIF_CHAN_U], nullptr);
        EXPECT_EQ(image->yuvPlanes[AVIF_CHAN_V], nullptr);
      }
    } else {
      EXPECT_EQ(image->yuvPlanes[AVIF_CHAN_Y], nullptr);
      EXPECT_EQ(image->yuvPlanes[AVIF_CHAN_U], nullptr);
      EXPECT_EQ(image->yuvPlanes[AVIF_CHAN_V], nullptr);
    }
    if (planes & AVIF_PLANES_A) {
      EXPECT_NE(image->alphaPlane, nullptr);
    } else {
      EXPECT_EQ(image->alphaPlane, nullptr);
    }
  } else {
    ASSERT_NE(res, AVIF_RESULT_OK);
    EXPECT_EQ(image->yuvPlanes[AVIF_CHAN_Y], nullptr);
    EXPECT_EQ(image->yuvPlanes[AVIF_CHAN_U], nullptr);
    EXPECT_EQ(image->yuvPlanes[AVIF_CHAN_V], nullptr);
    EXPECT_EQ(image->alphaPlane, nullptr);
  }
}

class ImageAllocationTest
    : public testing::TestWithParam<
          std::tuple<avifPixelFormat, avifPlanesFlag, /*depth=*/int>> {};

TEST_P(ImageAllocationTest, VariousCases) {
  const auto& param = GetParam();
  const auto yuv_format = std::get<0>(param);
  const auto planes = std::get<1>(param);
  const auto depth = std::get<2>(param);
  // Minimum valid image dimensions.
  TestAllocation(1, 1, depth, yuv_format, planes, true);
#if !defined(CRABBYAVIF_SANITIZER_BUILD)
  // Maximum valid image dimensions. This allocation is too large for
  // sanitizers.
  TestAllocation(CRABBY_AVIF_DEFAULT_IMAGE_DIMENSION_LIMIT,
                 CRABBY_AVIF_DEFAULT_IMAGE_DIMENSION_LIMIT, depth, yuv_format,
                 planes, true);
#endif
  // Invalid (too large).
  TestAllocation((1 << 30), 1, depth, yuv_format, planes, false);
}

INSTANTIATE_TEST_SUITE_P(
    All, ImageAllocationTest,
    testing::Combine(
        testing::Values(AVIF_PIXEL_FORMAT_NONE, AVIF_PIXEL_FORMAT_YUV444,
                        AVIF_PIXEL_FORMAT_YUV422, AVIF_PIXEL_FORMAT_YUV420,
                        AVIF_PIXEL_FORMAT_YUV400),
        testing::Values(AVIF_PLANES_YUV, AVIF_PLANES_A, AVIF_PLANES_ALL),
        testing::Values(8, 10, 12)));

void TestEncoding(uint32_t width, uint32_t height, uint32_t depth,
                  avifResult expected_result) {
  ImagePtr image(avifImageCreateEmpty());
  ASSERT_NE(image, nullptr);
  image->width = width;
  image->height = height;
  image->depth = depth;
  image->yuvFormat = AVIF_PIXEL_FORMAT_YUV444;

  // This is a fairly high number of bytes that can safely be allocated in this
  // test. The goal is to have something to give to libavif but libavif should
  // return an error before attempting to read all of it, so it does not matter
  // if there are fewer bytes than the provided image dimensions.
  static constexpr uint64_t kMaxAlloc = 1073741824;
  uint32_t row_bytes;
  size_t num_allocated_bytes;
  if (static_cast<uint64_t>(image->width) * image->height >
      kMaxAlloc / (avifImageUsesU16(image.get()) ? 2 : 1)) {
    row_bytes = 1024;  // Does not matter much.
    num_allocated_bytes = kMaxAlloc;
  } else {
    row_bytes = image->width * (avifImageUsesU16(image.get()) ? 2 : 1);
    num_allocated_bytes = row_bytes * image->height;
  }

  // Initialize pixels as 16b values to make sure values are valid for 10
  // and 12-bit depths. The array will be cast to uint8_t for 8-bit depth.
  std::vector<uint16_t> pixels(
      std::max(1lu, num_allocated_bytes / sizeof(uint16_t)), 400);
  uint8_t* bytes = reinterpret_cast<uint8_t*>(pixels.data());
  // Avoid avifImageAllocatePlanes() to exercise the checks at encoding.
  image->imageOwnsYUVPlanes = AVIF_FALSE;
  image->imageOwnsAlphaPlane = AVIF_FALSE;
  image->yuvRowBytes[AVIF_CHAN_Y] = row_bytes;
  image->yuvPlanes[AVIF_CHAN_Y] = bytes;
  image->yuvRowBytes[AVIF_CHAN_U] = row_bytes;
  image->yuvPlanes[AVIF_CHAN_U] = bytes;
  image->yuvRowBytes[AVIF_CHAN_V] = row_bytes;
  image->yuvPlanes[AVIF_CHAN_V] = bytes;
  image->alphaRowBytes = row_bytes;
  image->alphaPlane = bytes;

  // Try to encode.
  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  encoder->speed = 10;
  AvifRwData encoded_avif;
  ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded_avif),
            expected_result);
}

TEST(EncodingTest, VariousCases) {
  TestEncoding(1, 1, 8, AVIF_RESULT_OK);
  TestEncoding(101, 102, 8, AVIF_RESULT_OK);
#if !defined(CRABBYAVIF_SANITIZER_BUILD)
  // This allocation is too large for sanitizers.
  TestEncoding(CRABBY_AVIF_DEFAULT_IMAGE_DIMENSION_LIMIT / 2,
               CRABBY_AVIF_DEFAULT_IMAGE_DIMENSION_LIMIT / 2, 8,
               AVIF_RESULT_OK);
#endif
}

TEST(ImageTest, MetadataFunctions) {
  // Fill data with some values.
  constexpr int kSize = 10;
  std::vector<uint8_t> data;
  data.reserve(kSize);
  for (int i = 0; i < kSize; ++i) {
    data.push_back(i * 2);
  }

  ImagePtr image(avifImageCreateEmpty());
  ASSERT_NE(image, nullptr);

  for (auto* func : {avifImageSetMetadataExif, avifImageSetProfileICC,
                     avifImageSetMetadataXMP}) {
    const auto& metadata =
        (func == avifImageSetMetadataExif)
            ? image->exif
            : ((func == avifImageSetProfileICC) ? image->icc : image->xmp);
    EXPECT_NE(func(nullptr, nullptr, 0), AVIF_RESULT_OK);
    EXPECT_EQ(func(image.get(), nullptr, 0), AVIF_RESULT_OK);
    EXPECT_EQ(metadata.data, nullptr);
    EXPECT_EQ(metadata.size, 0);
    EXPECT_EQ(func(image.get(), data.data(), 0), AVIF_RESULT_OK);
    EXPECT_EQ(metadata.data, nullptr);
    EXPECT_EQ(metadata.size, 0);
    EXPECT_EQ(func(image.get(), data.data(), kSize), AVIF_RESULT_OK);
    EXPECT_NE(metadata.data, nullptr);
    EXPECT_NE(metadata.data, data.data());
    ASSERT_EQ(metadata.size, kSize);
    EXPECT_EQ(memcmp(metadata.data, data.data(), kSize), 0);
  }
}

TEST(ImageTest, NullCases) {
  ImagePtr src(avifImageCreateEmpty());
  ImagePtr dst(avifImageCreateEmpty());
  // Both dst and src are nullptr.
  EXPECT_NE(avifImageCopy(nullptr, nullptr, AVIF_PLANES_ALL), AVIF_RESULT_OK);
  // src is nullptr.
  EXPECT_NE(avifImageCopy(dst.get(), nullptr, AVIF_PLANES_ALL), AVIF_RESULT_OK);
  // dst is nullptr.
  EXPECT_NE(avifImageCopy(nullptr, src.get(), AVIF_PLANES_ALL), AVIF_RESULT_OK);

  EXPECT_NE(avifImageAllocatePlanes(nullptr, AVIF_PLANES_ALL), AVIF_RESULT_OK);

  // This should not crash.
  avifImageFreePlanes(nullptr, AVIF_PLANES_ALL);

  // This should not crash.
  avifImageDestroy(nullptr);

  EXPECT_FALSE(avifImageUsesU16(nullptr));
  EXPECT_FALSE(avifImageIsOpaque(nullptr));
  EXPECT_EQ(avifImagePlane(nullptr, 0), nullptr);
  EXPECT_EQ(avifImagePlaneRowBytes(nullptr, 0), 0);
  EXPECT_EQ(avifImagePlaneWidth(nullptr, 0), 0);
  EXPECT_EQ(avifImagePlaneHeight(nullptr, 0), 0);

  // Various nullptr combinations for avifImageSetViewRect.
  EXPECT_NE(avifImageSetViewRect(nullptr, nullptr, nullptr), AVIF_RESULT_OK);

  EXPECT_NE(avifRWDataRealloc(nullptr, 10), AVIF_RESULT_OK);

  avifRWData rw_data;
  uint8_t raw_data[10] = {0};
  EXPECT_NE(avifRWDataSet(nullptr, nullptr, 10), AVIF_RESULT_OK);
  EXPECT_NE(avifRWDataSet(&rw_data, nullptr, 10), AVIF_RESULT_OK);
  EXPECT_NE(avifRWDataSet(nullptr, raw_data, 10), AVIF_RESULT_OK);

  // This should not crash.
  avifRWDataFree(nullptr);

  EXPECT_EQ(avifIOCreateMemoryReader(nullptr, 10), nullptr);
  EXPECT_EQ(avifIOCreateFileReader(nullptr), nullptr);

  // This should not crash.
  avifIODestroy(nullptr);
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
