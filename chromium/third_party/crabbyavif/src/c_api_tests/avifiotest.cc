// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <iostream>

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

std::string get_file_name() {
  const char* file_name = "colors-animated-8bpc.avif";
  return std::string(data_path) + file_name;
}

TEST(AvifDecodeTest, SetRawIO) {
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  auto data = testutil::read_file(get_file_name().c_str());
  ASSERT_EQ(avifDecoderSetIOMemory(decoder.get(), data.data(), data.size()),
            AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  EXPECT_EQ(decoder->alphaPresent, AVIF_FALSE);
  EXPECT_EQ(decoder->imageSequenceTrackPresent, AVIF_TRUE);
  EXPECT_EQ(decoder->imageCount, 5);
  EXPECT_EQ(decoder->repetitionCount, 0);
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);
  }
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

TEST(AvifDecodeTest, SetCustomIO) {
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  auto data = testutil::read_file(get_file_name().c_str());
  avifROData ro_data = {.data = data.data(), .size = data.size()};
  avifIO io = {.destroy = nullptr,
               .read = io_read,
               .sizeHint = data.size(),
               .persistent = false,
               .data = static_cast<void*>(&ro_data)};
  avifDecoderSetIO(decoder.get(), &io);
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  EXPECT_EQ(decoder->alphaPresent, AVIF_FALSE);
  EXPECT_EQ(decoder->imageSequenceTrackPresent, AVIF_TRUE);
  EXPECT_EQ(decoder->imageCount, 5);
  EXPECT_EQ(decoder->repetitionCount, 0);
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);
  }
}

TEST(AvifDecodeTest, IOMemoryReader) {
  auto data = testutil::read_file(get_file_name().c_str());
  avifIO* io = avifIOCreateMemoryReader(data.data(), data.size());
  ASSERT_NE(io, nullptr);
  EXPECT_EQ(io->sizeHint, data.size());
  avifROData ro_data;
  // Read 10 bytes from the beginning.
  io->read(io, 0, 0, 10, &ro_data);
  EXPECT_EQ(ro_data.size, 10);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(ro_data.data[i], data[i]);
  }
  // Read 10 bytes from the middle.
  io->read(io, 0, 50, 10, &ro_data);
  EXPECT_EQ(ro_data.size, 10);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(ro_data.data[i], data[i + 50]);
  }
  avifIODestroy(io);
}

TEST(AvifDecodeTest, IOFileReader) {
  auto data = testutil::read_file(get_file_name().c_str());
  avifIO* io = avifIOCreateFileReader(get_file_name().c_str());
  ASSERT_NE(io, nullptr);
  EXPECT_EQ(io->sizeHint, data.size());
  avifROData ro_data;
  // Read 10 bytes from the beginning.
  io->read(io, 0, 0, 10, &ro_data);
  EXPECT_EQ(ro_data.size, 10);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(ro_data.data[i], data[i]);
  }
  // Read 10 bytes from the middle.
  io->read(io, 0, 50, 10, &ro_data);
  EXPECT_EQ(ro_data.size, 10);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(ro_data.data[i], data[i + 50]);
  }
  avifIODestroy(io);
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
