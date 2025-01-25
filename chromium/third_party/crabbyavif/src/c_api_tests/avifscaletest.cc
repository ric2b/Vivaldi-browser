// Copyright 2024 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <cstdint>
#include <iostream>
#include <string>

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

class ScaleTest : public testing::TestWithParam<const char*> {};

TEST_P(ScaleTest, Scaling) {
  if (!testutil::Av1DecoderAvailable()) {
    GTEST_SKIP() << "AV1 Codec unavailable, skip test.";
  }
  const char* file_name = GetParam();
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  ASSERT_EQ(avifDecoderSetIOFile(decoder.get(),
                                 (std::string(data_path) + file_name).c_str()),
            AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  EXPECT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);

  const uint32_t scaled_width =
      static_cast<uint32_t>(decoder->image->width * 0.8);
  const uint32_t scaled_height =
      static_cast<uint32_t>(decoder->image->height * 0.8);

  ASSERT_EQ(
      avifImageScale(decoder->image, scaled_width, scaled_height, nullptr),
      AVIF_RESULT_OK);
  EXPECT_EQ(decoder->image->width, scaled_width);
  EXPECT_EQ(decoder->image->height, scaled_height);

  // Scaling to a larger dimension is not supported.
  EXPECT_NE(avifImageScale(decoder->image, decoder->image->width * 2,
                           decoder->image->height * 0.5, nullptr),
            AVIF_RESULT_OK);
  EXPECT_NE(avifImageScale(decoder->image, decoder->image->width * 0.5,
                           decoder->image->height * 2, nullptr),
            AVIF_RESULT_OK);
  EXPECT_NE(avifImageScale(decoder->image, decoder->image->width * 2,
                           decoder->image->height * 2, nullptr),
            AVIF_RESULT_OK);
}

INSTANTIATE_TEST_SUITE_P(Some, ScaleTest,
                         testing::ValuesIn({"paris_10bpc.avif",
                                            "paris_icc_exif_xmp.avif"}));

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
