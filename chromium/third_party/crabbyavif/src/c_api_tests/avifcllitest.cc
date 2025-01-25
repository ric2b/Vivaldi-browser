// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

TEST(ClliTest, Simple) {
  struct Params {
    const char* file_name;
    uint32_t maxCLL;
    uint32_t maxPALL;
  };
  Params params[9] = {
    {"clli/clli_0_0.avif", 0, 0},
    {"clli/clli_0_1.avif", 0, 1},
    {"clli/clli_0_65535.avif", 0, 65535},
    {"clli/clli_1_0.avif", 1, 0},
    {"clli/clli_1_1.avif", 1, 1},
    {"clli/clli_1_65535.avif", 1, 65535},
    {"clli/clli_65535_0.avif", 65535, 0},
    {"clli/clli_65535_1.avif", 65535, 1},
    {"clli/clli_65535_65535.avif", 65535, 65535},
  };
  for (const auto& param : params) {
    DecoderPtr decoder(avifDecoderCreate());
    ASSERT_NE(decoder, nullptr);
    decoder->allowProgressive = true;
    ASSERT_EQ(avifDecoderSetIOFile(decoder.get(),
                                   (std::string(data_path) + param.file_name).c_str()),
              AVIF_RESULT_OK);
    ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
    avifImage* decoded = decoder->image;
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->clli.maxCLL, param.maxCLL);
    ASSERT_EQ(decoded->clli.maxPALL, param.maxPALL);
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