// Copyright 2022 Yuan Tong. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

TEST(AvifDecodeTest, Progressive) {
  struct Params {
    const char* file_name;
    uint32_t width;
    uint32_t height;
    uint32_t layer_count;
  };
  Params params[] = {
    {"progressive/progressive_dimension_change.avif", 256, 256, 2},
    {"progressive/progressive_layered_grid.avif", 512, 256, 2},
    {"progressive/progressive_quality_change.avif", 256, 256, 2},
    {"progressive/progressive_same_layers.avif", 256, 256, 4},
    {"progressive/tiger_3layer_1res.avif", 1216, 832, 3},
    {"progressive/tiger_3layer_3res.avif", 1216, 832, 3},
  };
  for (const auto& param : params) {
    DecoderPtr decoder(avifDecoderCreate());
    ASSERT_NE(decoder, nullptr);
    decoder->allowProgressive = true;
    ASSERT_EQ(avifDecoderSetIOFile(decoder.get(),
                                   (std::string(data_path) + param.file_name).c_str()),
              AVIF_RESULT_OK);
    ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
    ASSERT_EQ(decoder->progressiveState, AVIF_PROGRESSIVE_STATE_ACTIVE);
    ASSERT_EQ(static_cast<uint32_t>(decoder->imageCount), param.layer_count);

    for (uint32_t layer = 0; layer < param.layer_count; ++layer) {
      ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);
      // libavif scales frame automatically.
      ASSERT_EQ(decoder->image->width, param.width);
      ASSERT_EQ(decoder->image->height, param.height);
    }
  }
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
