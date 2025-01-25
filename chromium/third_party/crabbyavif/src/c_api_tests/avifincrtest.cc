// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <fstream>
#include <iostream>
#include <string>

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

using testing::Bool;
using testing::Combine;
using testing::Values;

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

// Verifies that the first (top) row_count rows of image1 and image2 are
// identical.
void ComparePartialYuva(const avifImage& image1, const avifImage& image2,
                        uint32_t row_count) {
  if (row_count == 0) {
    return;
  }
  ASSERT_EQ(image1.width, image2.width);
  ASSERT_GE(image1.height, row_count);
  ASSERT_GE(image2.height, row_count);
  ASSERT_EQ(image1.depth, image2.depth);
  ASSERT_EQ(image1.yuvFormat, image2.yuvFormat);
  ASSERT_EQ(image1.yuvRange, image2.yuvRange);

  avifPixelFormatInfo info;
  avifGetPixelFormatInfo(image1.yuvFormat, &info);
  const uint32_t uv_height =
      info.monochrome ? 0
                      : ((row_count + info.chromaShiftY) >> info.chromaShiftY);
  const size_t pixel_byte_count =
      (image1.depth > 8) ? sizeof(uint16_t) : sizeof(uint8_t);

  if (image1.alphaPlane) {
    ASSERT_NE(image2.alphaPlane, nullptr);
    ASSERT_EQ(image1.alphaPremultiplied, image2.alphaPremultiplied);
  }

  const int last_plane = image1.alphaPlane ? AVIF_CHAN_A : AVIF_CHAN_V;
  for (int plane = AVIF_CHAN_Y; plane <= last_plane; ++plane) {
    const size_t width_byte_count =
        avifImagePlaneWidth(&image1, plane) * pixel_byte_count;
    const uint32_t height =
        (plane == AVIF_CHAN_Y || plane == AVIF_CHAN_A) ? row_count : uv_height;
    const uint8_t* row1 = avifImagePlane(&image1, plane);
    ASSERT_NE(row1, nullptr);
    const uint8_t* row2 = avifImagePlane(&image2, plane);
    ASSERT_NE(row2, nullptr);
    const uint32_t row1_bytes = avifImagePlaneRowBytes(&image1, plane);
    const uint32_t row2_bytes = avifImagePlaneRowBytes(&image2, plane);
    for (uint32_t y = 0; y < height; ++y) {
      ASSERT_EQ(memcmp(row1, row2, width_byte_count), 0);
      row1 += row1_bytes;
      row2 += row2_bytes;
    }
  }
}

// Returns the expected number of decoded rows when available_byte_count out of
// byte_count were given to the decoder, for an image of height rows, split into
// cells of cell_height rows.
uint32_t GetMinDecodedRowCount(uint32_t height, uint32_t cell_height,
                               bool has_alpha, size_t available_byte_count,
                               size_t byte_count,
                               bool enable_fine_incremental_check) {
  // The whole image should be available when the full input is.
  if (available_byte_count >= byte_count) {
    return height;
  }
  // All but one cell should be decoded if at most 10 bytes are missing.
  if ((available_byte_count + 10) >= byte_count) {
    return height - cell_height;
  }

  // The tests below can be hard to tune for any kind of input, especially
  // fuzzed grids. Early exit in that case.
  if (!enable_fine_incremental_check) return 0;

  // Subtract the header because decoding it does not output any pixel.
  // Most AVIF headers are below 500 bytes.
  if (available_byte_count <= 500) {
    return 0;
  }
  available_byte_count -= 500;
  byte_count -= 500;
  // Alpha, if any, is assumed to be located before the other planes and to
  // represent at most 50% of the payload.
  if (has_alpha) {
    if (available_byte_count <= (byte_count / 2)) {
      return 0;
    }
    available_byte_count -= byte_count / 2;
    byte_count -= byte_count / 2;
  }
  // Linearly map the input availability ratio to the decoded row ratio.
  const uint32_t min_decoded_cell_row_count = static_cast<uint32_t>(
      (height / cell_height) * available_byte_count / byte_count);
  const uint32_t min_decoded_px_row_count =
      min_decoded_cell_row_count * cell_height;
  // One cell is the incremental decoding granularity.
  // It is unlikely that bytes are evenly distributed among cells. Offset two of
  // them.
  if (min_decoded_px_row_count <= (2 * cell_height)) {
    return 0;
  }
  return min_decoded_px_row_count - 2 * cell_height;
}

struct PartialData {
  avifROData available;
  size_t full_size;

  // Only used as nonpersistent input.
  std::unique_ptr<uint8_t[]> nonpersistent_bytes;
  size_t num_nonpersistent_bytes;
};

// Implementation of avifIOReadFunc simulating a stream from an array. See
// avifIOReadFunc documentation. io->data is expected to point to PartialData.
avifResult PartialRead(struct avifIO* io, uint32_t read_flags,
                       uint64_t offset64, size_t size, avifROData* out) {
  PartialData* data = reinterpret_cast<PartialData*>(io->data);
  if ((read_flags != 0) || !data || (data->full_size < offset64)) {
    return AVIF_RESULT_IO_ERROR;
  }
  const size_t offset = static_cast<size_t>(offset64);
  // Use |offset| instead of |offset64| from this point on.
  if (size > (data->full_size - offset)) {
    size = data->full_size - offset;
  }
  if (data->available.size < (offset + size)) {
    return AVIF_RESULT_WAITING_ON_IO;
  }
  if (io->persistent) {
    out->data = data->available.data + offset;
  } else {
    // Dedicated buffer containing just the available bytes and nothing more.
    std::unique_ptr<uint8_t[]> bytes(new uint8_t[size]);
    std::copy(data->available.data + offset,
              data->available.data + offset + size, bytes.get());
    out->data = bytes.get();
    // Flip the previously returned bytes to make sure the values changed.
    for (size_t i = 0; i < data->num_nonpersistent_bytes; ++i) {
      data->nonpersistent_bytes[i] = ~data->nonpersistent_bytes[i];
    }
    // Free the memory to invalidate the old pointer. Only do that after
    // allocating the new bytes to make sure to have a different pointer.
    data->nonpersistent_bytes = std::move(bytes);
    data->num_nonpersistent_bytes = size;
  }
  out->size = size;
  return AVIF_RESULT_OK;
}

avifResult DecodeIncrementally(const avifRWData& encoded_avif,
                               avifDecoder* decoder, bool is_persistent,
                               bool give_size_hint, bool use_nth_image_api,
                               const avifImage& reference, uint32_t cell_height,
                               bool enable_fine_incremental_check,
                               bool expect_whole_file_read) {
  // AVIF cells are at least 64 pixels tall.
  if (cell_height != reference.height) {
    AVIF_CHECKERR(cell_height >= 64u, AVIF_RESULT_INVALID_ARGUMENT);
  }

  // Emulate a byte-by-byte stream.
  PartialData data = {
      /*available=*/{encoded_avif.data, 0}, /*fullSize=*/encoded_avif.size,
      /*nonpersistent_bytes=*/nullptr, /*num_nonpersistent_bytes=*/0};
  avifIO io = {
      /*destroy=*/nullptr, PartialRead,
      /*write=*/nullptr,   give_size_hint ? encoded_avif.size : 0,
      is_persistent,       &data};
  avifDecoderSetIO(decoder, &io);
  decoder->allowIncremental = AVIF_TRUE;
  const size_t step = std::max<size_t>(1, data.full_size / 10000);

  // Parsing is not incremental.
  avifResult parse_result = avifDecoderParse(decoder);
  while (parse_result == AVIF_RESULT_WAITING_ON_IO) {
    if (data.available.size >= data.full_size) {
      std::cerr << "avifDecoderParse() returned WAITING_ON_IO instead of OK"
                << std::endl;
      return AVIF_RESULT_TRUNCATED_DATA;
    }
    data.available.size = std::min(data.available.size + step, data.full_size);
    parse_result = avifDecoderParse(decoder);
  }
  EXPECT_EQ(parse_result, AVIF_RESULT_OK);

  // Decoding is incremental.
  uint32_t previously_decoded_row_count = 0;
  avifResult next_image_result = use_nth_image_api
                                     ? avifDecoderNextImage(decoder)
                                     : avifDecoderNextImage(decoder);
  while (next_image_result == AVIF_RESULT_WAITING_ON_IO) {
    if (data.available.size >= data.full_size) {
      std::cerr << (use_nth_image_api ? "avifDecoderNthImage(0)"
                                      : "avifDecoderNextImage()")
                << " returned WAITING_ON_IO instead of OK";
      return AVIF_RESULT_INVALID_ARGUMENT;
    }
    const uint32_t decoded_row_count = avifDecoderDecodedRowCount(decoder);
    EXPECT_GE(decoded_row_count, previously_decoded_row_count);
    const uint32_t min_decoded_row_count = GetMinDecodedRowCount(
        reference.height, cell_height, reference.alphaPlane != nullptr,
        data.available.size, data.full_size, enable_fine_incremental_check);
    EXPECT_GE(decoded_row_count, min_decoded_row_count);
    if (decoded_row_count > 0) {
      ComparePartialYuva(reference, *decoder->image, decoded_row_count);
    }

    previously_decoded_row_count = decoded_row_count;
    data.available.size = std::min(data.available.size + step, data.full_size);
    next_image_result = use_nth_image_api ? avifDecoderNextImage(decoder)
                                          : avifDecoderNextImage(decoder);
  }
  EXPECT_EQ(next_image_result, AVIF_RESULT_OK);
  if (expect_whole_file_read) {
    EXPECT_EQ(data.available.size, data.full_size);
  }
  EXPECT_EQ(avifDecoderDecodedRowCount(decoder), decoder->image->height);

  ComparePartialYuva(reference, *decoder->image, reference.height);
  return AVIF_RESULT_OK;
}

std::string get_file_name(const char* file_name) {
  return std::string(data_path) + file_name;
}

// Check that non-incremental and incremental decodings of a grid AVIF produce
// the same pixels.
TEST(IncrementalTest, Decode) {
  auto file_data =
      testutil::read_file(get_file_name("sofa_grid1x5_420.avif").c_str());
  avifRWData encoded_avif = {.data = file_data.data(),
                             .size = file_data.size()};
  ASSERT_NE(encoded_avif.size, 0u);
  ImagePtr reference(avifImageCreateEmpty());
  ASSERT_NE(reference, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  ASSERT_EQ(avifDecoderReadMemory(decoder.get(), reference.get(),
                                  encoded_avif.data, encoded_avif.size),
            AVIF_RESULT_OK);

  DecoderPtr decoder2(avifDecoderCreate());
  ASSERT_NE(decoder2, nullptr);

  // Cell height is hardcoded because there is no API to extract it from an
  // encoded payload.
  ASSERT_EQ(DecodeIncrementally(encoded_avif, decoder2.get(),
                                /*is_persistent=*/true, /*give_size_hint=*/true,
                                /*use_nth_image_api=*/false, *reference,
                                /*cell_height=*/154,
                                /*enable_fine_incremental_check=*/true, true),
            AVIF_RESULT_OK);
}

}  // namespace
}  // namespace avif

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc < 2) {
    std::cerr
        << "The path to the test data folder must be provided as an argument"
        << std::endl;
    return 1;
  }
  avif::data_path = argv[1];
  return RUN_ALL_TESTS();
}
