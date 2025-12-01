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
#include "testutil.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <limits>
#include <vector>

#include "avif/avif.h"
#include "avif/libavif_compat.h"
#include "gtest/gtest.h"

using namespace crabbyavif;

namespace avif {

AvifRgbImage::AvifRgbImage(const avifImage* yuv, int rgbDepth,
                           avifRGBFormat rgbFormat) {
  avifRGBImageSetDefaults(this, yuv);
  depth = rgbDepth;
  format = rgbFormat;
  if (avifRGBImageAllocatePixels(this) != AVIF_RESULT_OK) {
    std::abort();
  }
}

}  // namespace avif

namespace testutil {
namespace {

template <typename Sample>
uint64_t SquaredDiffSum(const Sample* samples1, const Sample* samples2,
                        uint32_t num_samples) {
  uint64_t sum = 0;
  for (uint32_t i = 0; i < num_samples; ++i) {
    const int32_t diff = static_cast<int32_t>(samples1[i]) - samples2[i];
    sum += diff * diff;
  }
  return sum;
}

template <typename PixelType>
void FillImageChannel(avifRGBImage* image, uint32_t channel_offset,
                      uint32_t value) {
  const uint32_t channel_count = avifRGBFormatChannelCount(image->format);
  for (uint32_t y = 0; y < image->height; ++y) {
    PixelType* pixel =
        reinterpret_cast<PixelType*>(image->pixels + image->rowBytes * y);
    for (uint32_t x = 0; x < image->width; ++x) {
      pixel[channel_offset] = static_cast<PixelType>(value);
      pixel += channel_count;
    }
  }
}

// Modifies the pixel values of a channel in image by modifier[] (row-ordered).
template <typename PixelType>
void ModifyImageChannel(avifRGBImage* image, uint32_t channel_offset,
                        const uint8_t modifier[kModifierSize]) {
  const uint32_t channel_count = avifRGBFormatChannelCount(image->format);
  for (uint32_t y = 0, i = 0; y < image->height; ++y) {
    PixelType* pixel =
        reinterpret_cast<PixelType*>(image->pixels + image->rowBytes * y);
    for (uint32_t x = 0; x < image->width; ++x, ++i) {
      pixel[channel_offset] += modifier[i % kModifierSize];
      pixel += channel_count;
    }
  }
}

}  // namespace

std::vector<uint8_t> read_file(const char* file_name) {
  std::ifstream file(file_name, std::ios::binary);
  EXPECT_TRUE(file.is_open());
  // Get file size.
  file.seekg(0, std::ios::end);
  auto size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> data(size);
  file.read(reinterpret_cast<char*>(data.data()), size);
  file.close();
  return data;
}

crabbyavif::ImagePtr CreateImage(int width, int height, int depth,
                                 avifPixelFormat yuv_format,
                                 avifPlanesFlags planes, avifRange yuv_range) {
  crabbyavif::ImagePtr image(avifImageCreate(width, height, depth, yuv_format));
  if (!image) {
    return nullptr;
  }
  image->yuvRange = yuv_range;
  if (avifImageAllocatePlanes(image.get(), planes) != AVIF_RESULT_OK) {
    return nullptr;
  }
  return image;
}

void FillImageGradient(avifImage* image, int offset) {
  for (avifChannelIndex c :
       {AVIF_CHAN_Y, AVIF_CHAN_U, AVIF_CHAN_V, AVIF_CHAN_A}) {
    const uint32_t limitedRangeMin =
        c == AVIF_CHAN_Y ? 16 << (image->depth - 8) : 0;
    const uint32_t limitedRangeMax = (c == AVIF_CHAN_Y ? 219 : 224)
                                     << (image->depth - 8);

    const uint32_t plane_width = avifImagePlaneWidth(image, c);
    // 0 for A if no alpha and 0 for UV if 4:0:0.
    const uint32_t plane_height = avifImagePlaneHeight(image, c);
    uint8_t* row = avifImagePlane(image, c);
    const uint32_t row_bytes = avifImagePlaneRowBytes(image, c);
    const uint32_t max_xy_sum = plane_width + plane_height - 2;
    for (uint32_t y = 0; y < plane_height; ++y) {
      for (uint32_t x = 0; x < plane_width; ++x) {
        uint32_t value = (x + y + offset) % (max_xy_sum + 1);
        if (image->yuvRange == AVIF_RANGE_FULL || c == AVIF_CHAN_A) {
          value =
              value * ((1u << image->depth) - 1u) / std::max(1u, max_xy_sum);
        } else {
          value = limitedRangeMin + value *
                                        (limitedRangeMax - limitedRangeMin) /
                                        std::max(1u, max_xy_sum);
        }
        if (avifImageUsesU16(image)) {
          reinterpret_cast<uint16_t*>(row)[x] = static_cast<uint16_t>(value);
        } else {
          row[x] = static_cast<uint8_t>(value);
        }
      }
      row += row_bytes;
    }
  }
}

double GetPsnr(const avifImage& image1, const avifImage& image2,
               bool ignore_alpha) {
  if (image1.width != image2.width || image1.height != image2.height ||
      image1.depth != image2.depth || image1.yuvFormat != image2.yuvFormat ||
      image1.yuvRange != image2.yuvRange) {
    return -1.0;
  }
  uint64_t squared_diff_sum = 0;
  uint32_t num_samples = 0;
  const uint32_t max_sample_value = (1 << image1.depth) - 1;
  for (avifChannelIndex c :
       {AVIF_CHAN_Y, AVIF_CHAN_U, AVIF_CHAN_V, AVIF_CHAN_A}) {
    if (ignore_alpha && c == AVIF_CHAN_A) continue;

    const uint32_t plane_width = std::max(avifImagePlaneWidth(&image1, c),
                                          avifImagePlaneWidth(&image2, c));
    const uint32_t plane_height = std::max(avifImagePlaneHeight(&image1, c),
                                           avifImagePlaneHeight(&image2, c));
    if (plane_width == 0 || plane_height == 0) continue;

    const uint8_t* row1 = avifImagePlane(&image1, c);
    const uint8_t* row2 = avifImagePlane(&image2, c);
    if (!row1 != !row2 && c != AVIF_CHAN_A) {
      return -1.0;
    }
    uint32_t row_bytes1 = avifImagePlaneRowBytes(&image1, c);
    uint32_t row_bytes2 = avifImagePlaneRowBytes(&image2, c);

    // Consider missing alpha planes as samples set to the maximum value.
    std::vector<uint8_t> opaque_alpha_samples;
    if (!row1 != !row2) {
      opaque_alpha_samples.resize(std::max(row_bytes1, row_bytes2));
      if (avifImageUsesU16(&image1)) {
        uint16_t* opaque_alpha_samples_16b =
            reinterpret_cast<uint16_t*>(opaque_alpha_samples.data());
        std::fill(opaque_alpha_samples_16b,
                  opaque_alpha_samples_16b + plane_width,
                  static_cast<int16_t>(max_sample_value));
      } else {
        std::fill(opaque_alpha_samples.begin(), opaque_alpha_samples.end(),
                  uint8_t{255});
      }
      if (!row1) {
        row1 = opaque_alpha_samples.data();
        row_bytes1 = 0;
      } else {
        row2 = opaque_alpha_samples.data();
        row_bytes2 = 0;
      }
    }

    for (uint32_t y = 0; y < plane_height; ++y) {
      if (avifImageUsesU16(&image1)) {
        squared_diff_sum += SquaredDiffSum(
            reinterpret_cast<const uint16_t*>(row1),
            reinterpret_cast<const uint16_t*>(row2), plane_width);
      } else {
        squared_diff_sum += SquaredDiffSum(row1, row2, plane_width);
      }
      row1 += row_bytes1;
      row2 += row_bytes2;
      num_samples += plane_width;
    }
  }

  if (squared_diff_sum == 0) {
    return 99.0;
  }
  const double normalized_error =
      squared_diff_sum /
      (static_cast<double>(num_samples) * max_sample_value * max_sample_value);
  if (normalized_error <= std::numeric_limits<double>::epsilon()) {
    return 98.99;  // Very small distortion but not lossless.
  }
  return std::min(-10 * std::log10(normalized_error), 98.99);
}

bool AreByteSequencesEqual(const uint8_t* data1, size_t data1_length,
                           const uint8_t* data2, size_t data2_length) {
  if (data1_length != data2_length) return false;
  return data1_length == 0 || std::equal(data1, data1 + data1_length, data2);
}

bool AreByteSequencesEqual(const avifRWData& data1, const avifRWData& data2) {
  return AreByteSequencesEqual(data1.data, data1.size, data2.data, data2.size);
}

bool ArePlanesEqual(const avifImage& image1, const avifImage& image2,
                    avifChannelIndex c) {
  if (image1.width != image2.width || image1.height != image2.height ||
      image1.depth != image2.depth || image1.yuvFormat != image2.yuvFormat ||
      image1.yuvRange != image2.yuvRange) {
    return false;
  }

  const uint8_t* row1 = avifImagePlane(&image1, c);
  const uint8_t* row2 = avifImagePlane(&image2, c);
  if (!row1 != !row2) {
    return false;
  }
  if (c == AVIF_CHAN_A && row1 != nullptr &&
      image1.alphaPremultiplied != image2.alphaPremultiplied) {
    return false;
  }
  const uint32_t row_bytes1 = avifImagePlaneRowBytes(&image1, c);
  const uint32_t row_bytes2 = avifImagePlaneRowBytes(&image2, c);
  const uint32_t plane_width = avifImagePlaneWidth(&image1, c);
  const uint32_t plane_height = avifImagePlaneHeight(&image1, c);
  for (uint32_t y = 0; y < plane_height; ++y) {
    if (avifImageUsesU16(&image1)) {
      if (!std::equal(reinterpret_cast<const uint16_t*>(row1),
                      reinterpret_cast<const uint16_t*>(row1) + plane_width,
                      reinterpret_cast<const uint16_t*>(row2))) {
        return false;
      }
    } else {
      if (!std::equal(row1, row1 + plane_width, row2)) {
        return false;
      }
    }
    row1 += row_bytes1;
    row2 += row_bytes2;
  }
  return true;
}

bool AreImagesEqual(const avifImage& image1, const avifImage& image2,
                    bool ignore_alpha) {
  if (image1.width != image2.width || image1.height != image2.height ||
      image1.depth != image2.depth || image1.yuvFormat != image2.yuvFormat ||
      image1.yuvRange != image2.yuvRange) {
    return false;
  }

  for (avifChannelIndex c :
       {AVIF_CHAN_Y, AVIF_CHAN_U, AVIF_CHAN_V, AVIF_CHAN_A}) {
    if (ignore_alpha && c == AVIF_CHAN_A) continue;
    const uint8_t* row1 = avifImagePlane(&image1, c);
    const uint8_t* row2 = avifImagePlane(&image2, c);
    if (!row1 != !row2) {
      return false;
    }
    if (c == AVIF_CHAN_A && row1 != nullptr &&
        image1.alphaPremultiplied != image2.alphaPremultiplied) {
      return false;
    }
    const uint32_t row_bytes1 = avifImagePlaneRowBytes(&image1, c);
    const uint32_t row_bytes2 = avifImagePlaneRowBytes(&image2, c);
    const uint32_t plane_width = avifImagePlaneWidth(&image1, c);
    const uint32_t plane_height = avifImagePlaneHeight(&image1, c);
    for (uint32_t y = 0; y < plane_height; ++y) {
      if (avifImageUsesU16(&image1)) {
        if (!std::equal(reinterpret_cast<const uint16_t*>(row1),
                        reinterpret_cast<const uint16_t*>(row1) + plane_width,
                        reinterpret_cast<const uint16_t*>(row2))) {
          return false;
        }
      } else {
        if (!std::equal(row1, row1 + plane_width, row2)) {
          return false;
        }
      }
      row1 += row_bytes1;
      row2 += row_bytes2;
    }
  }

  if (!AreByteSequencesEqual(image1.icc, image2.icc)) return false;

  if (image1.colorPrimaries != image2.colorPrimaries ||
      image1.transferCharacteristics != image2.transferCharacteristics ||
      image1.matrixCoefficients != image2.matrixCoefficients) {
    return false;
  }

  if (image1.clli.maxCLL != image2.clli.maxCLL ||
      image1.clli.maxPALL != image2.clli.maxPALL) {
    return false;
  }
  if (image1.transformFlags != image2.transformFlags ||
      ((image1.transformFlags & AVIF_TRANSFORM_PASP) &&
       memcmp(&image1.pasp, &image2.pasp, sizeof(image1.pasp))) ||
      ((image1.transformFlags & AVIF_TRANSFORM_CLAP) &&
       memcmp(&image1.clap, &image2.clap, sizeof(image1.clap))) ||
      ((image1.transformFlags & AVIF_TRANSFORM_IROT) &&
       memcmp(&image1.irot, &image2.irot, sizeof(image1.irot))) ||
      ((image1.transformFlags & AVIF_TRANSFORM_IMIR) &&
       memcmp(&image1.imir, &image2.imir, sizeof(image1.imir)))) {
    return false;
  }

  if (!AreByteSequencesEqual(image1.exif, image2.exif)) return false;
  if (!AreByteSequencesEqual(image1.xmp, image2.xmp)) return false;

  if (!image1.gainMap != !image2.gainMap) return false;
  if (image1.gainMap != nullptr) {
    if (!image1.gainMap->image != !image2.gainMap->image) return false;
    if (image1.gainMap->image != nullptr &&
        !AreImagesEqual(*image1.gainMap->image, *image2.gainMap->image,
                        false)) {
      return false;
    }
  }
  return true;
}

namespace {

void CopyImageSamples(avifImage* dstImage, const avifImage* srcImage,
                      avifPlanesFlags planes) {
  const size_t bytesPerPixel = avifImageUsesU16(srcImage) ? 2 : 1;

  const avifBool skipColor = !(planes & AVIF_PLANES_YUV);
  const avifBool skipAlpha = !(planes & AVIF_PLANES_A);
  for (int c = AVIF_CHAN_Y; c <= AVIF_CHAN_A; ++c) {
    const avifBool alpha = c == AVIF_CHAN_A;
    if ((skipColor && !alpha) || (skipAlpha && alpha)) {
      continue;
    }

    const uint32_t planeWidth = avifImagePlaneWidth(srcImage, c);
    const uint32_t planeHeight = avifImagePlaneHeight(srcImage, c);
    const uint8_t* srcRow = avifImagePlane(srcImage, c);
    uint8_t* dstRow = avifImagePlane(dstImage, c);
    const uint32_t srcRowBytes = avifImagePlaneRowBytes(srcImage, c);
    const uint32_t dstRowBytes = avifImagePlaneRowBytes(dstImage, c);
    if (!srcRow) {
      continue;
    }

    const size_t planeWidthBytes = planeWidth * bytesPerPixel;
    for (uint32_t y = 0; y < planeHeight; ++y) {
      memcpy(dstRow, srcRow, planeWidthBytes);
      srcRow += srcRowBytes;
      dstRow += dstRowBytes;
    }
  }
}

}  // namespace

avifResult MergeGridFromRawPointers(int grid_cols, int grid_rows,
                                    const std::vector<const avifImage*>& cells,
                                    avifImage* merged) {
  const uint32_t tile_width = cells[0]->width;
  const uint32_t tile_height = cells[0]->height;
  const uint32_t grid_width =
      (grid_cols - 1) * tile_width + cells.back()->width;
  const uint32_t grid_height =
      (grid_rows - 1) * tile_height + cells.back()->height;

  crabbyavif::ImagePtr view(avifImageCreateEmpty());
  AVIF_CHECKERR(view, AVIF_RESULT_OUT_OF_MEMORY);

  avifCropRect rect = {};
  for (int j = 0; j < grid_rows; ++j) {
    rect.x = 0;
    for (int i = 0; i < grid_cols; ++i) {
      const avifImage* image = cells[j * grid_cols + i];
      rect.width = image->width;
      rect.height = image->height;
      AVIF_CHECKRES(avifImageSetViewRect(view.get(), merged, &rect));
      CopyImageSamples(/*dstImage=*/view.get(), image, AVIF_PLANES_ALL);
      rect.x += rect.width;
    }
    rect.y += rect.height;
  }

  if ((rect.x != grid_width) || (rect.y != grid_height)) {
    return AVIF_RESULT_UNKNOWN_ERROR;
  }

  return AVIF_RESULT_OK;
}

avifResult MergeGrid(int grid_cols, int grid_rows,
                     const std::vector<crabbyavif::ImagePtr>& cells,
                     avifImage* merged) {
  std::vector<const avifImage*> ptrs(cells.size());
  for (size_t i = 0; i < cells.size(); ++i) {
    ptrs[i] = cells[i].get();
  }
  return MergeGridFromRawPointers(grid_cols, grid_rows, ptrs, merged);
}

void FillImageChannel(avifRGBImage* image, uint32_t channel_offset,
                      uint32_t value) {
  (image->depth <= 8)
      ? FillImageChannel<uint8_t>(image, channel_offset, value)
      : FillImageChannel<uint16_t>(image, channel_offset, value);
}

void ModifyImageChannel(avifRGBImage* image, uint32_t channel_offset,
                        const uint8_t modifier[kModifierSize]) {
  if (image->depth <= 8) {
    ModifyImageChannel<uint8_t>(image, channel_offset, modifier);
  } else {
    ModifyImageChannel<uint16_t>(image, channel_offset, modifier);
  }
}

}  // namespace testutil
