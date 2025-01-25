// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

std::string get_file_name(const char* file_name) {
  return std::string(data_path) + file_name;
}

TEST(AvifDecodeTest, OneShotDecodeFile) {
  if (!testutil::Av1DecoderAvailable()) {
    GTEST_SKIP() << "AV1 Codec unavailable, skip test.";
  }
  const char* file_name = "sofa_grid1x5_420.avif";
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  avifImage image;
  ASSERT_EQ(avifDecoderReadFile(decoder.get(), &image,
                                get_file_name(file_name).c_str()),
            AVIF_RESULT_OK);
  EXPECT_EQ(image.width, 1024);
  EXPECT_EQ(image.height, 770);
  EXPECT_EQ(image.depth, 8);

  // TODO: Add test using same decoder with another read.
}

TEST(AvifDecodeTest, OneShotDecodeMemory) {
  if (!testutil::Av1DecoderAvailable()) {
    GTEST_SKIP() << "AV1 Codec unavailable, skip test.";
  }
  const char* file_name = "sofa_grid1x5_420.avif";
  auto file_data = testutil::read_file(get_file_name(file_name).c_str());
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  avifImage image;
  ASSERT_EQ(avifDecoderReadMemory(decoder.get(), &image, file_data.data(),
                                  file_data.size()),
            AVIF_RESULT_OK);
  EXPECT_EQ(image.width, 1024);
  EXPECT_EQ(image.height, 770);
  EXPECT_EQ(image.depth, 8);
}

avifResult io_read(struct avifIO* io, uint32_t flags, uint64_t offset,
                   size_t size, avifROData* out) {
  avifROData* src = (avifROData*)io->data;
  if (flags != 0 || offset > src->size) {
    return AVIF_RESULT_IO_ERROR;
  }
  uint64_t available_size = src->size - offset;
  if (size > available_size) {
    size = static_cast<size_t>(available_size);
  }
  out->data = src->data + offset;
  out->size = size;
  return AVIF_RESULT_OK;
}

TEST(AvifDecodeTest, OneShotDecodeCustomIO) {
  if (!testutil::Av1DecoderAvailable()) {
    GTEST_SKIP() << "AV1 Codec unavailable, skip test.";
  }
  const char* file_name = "sofa_grid1x5_420.avif";
  auto data = testutil::read_file(get_file_name(file_name).c_str());
  avifROData ro_data = {.data = data.data(), .size = data.size()};
  avifIO io = {.destroy = nullptr,
               .read = io_read,
               .sizeHint = data.size(),
               .persistent = false,
               .data = static_cast<void*>(&ro_data)};
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  avifDecoderSetIO(decoder.get(), &io);
  avifImage image;
  ASSERT_EQ(avifDecoderRead(decoder.get(), &image), AVIF_RESULT_OK);
  EXPECT_EQ(image.width, 1024);
  EXPECT_EQ(image.height, 770);
  EXPECT_EQ(image.depth, 8);
}

TEST(AvifDecodeTest, NthImage) {
  if (!testutil::Av1DecoderAvailable()) {
    GTEST_SKIP() << "AV1 Codec unavailable, skip test.";
  }
  const char* file_name = "colors-animated-8bpc.avif";
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  ASSERT_EQ(avifDecoderSetIOFile(decoder.get(),
                                 (std::string(data_path) + file_name).c_str()),
            AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  EXPECT_EQ(decoder->imageCount, 5);
  EXPECT_EQ(avifDecoderNthImage(decoder.get(), 3), AVIF_RESULT_OK);
  EXPECT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);
  EXPECT_NE(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);
  EXPECT_EQ(avifDecoderNthImage(decoder.get(), 1), AVIF_RESULT_OK);
  EXPECT_EQ(avifDecoderNthImage(decoder.get(), 4), AVIF_RESULT_OK);
  EXPECT_NE(avifDecoderNthImage(decoder.get(), 50), AVIF_RESULT_OK);
  for (int i = 0; i < 5; ++i) {
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
