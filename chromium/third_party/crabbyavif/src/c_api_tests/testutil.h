/*
 * Copyright 2024 Google LLC
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

#include <cstddef>
#include <cstdint>
#include <vector>

#include "avif/avif.h"
#include "avif/libavif_compat.h"

using namespace crabbyavif;

// Used instead of CHECK if needing to return a specific error on failure,
// instead of AVIF_FALSE
#define AVIF_CHECKERR(A, ERR) \
  do {                        \
    if (!(A)) {               \
      return ERR;             \
    }                         \
  } while (0)

// Forward any error to the caller now or continue execution.
#define AVIF_CHECKRES(A)              \
  do {                                \
    const avifResult result__ = (A);  \
    if (result__ != AVIF_RESULT_OK) { \
      return result__;                \
    }                                 \
  } while (0)

namespace avif {

class AvifRwData : public avifRWData {
 public:
  AvifRwData() : avifRWData{nullptr, 0} {}
  AvifRwData(const AvifRwData&) = delete;
  AvifRwData(AvifRwData&& other);
  ~AvifRwData() { avifRWDataFree(this); }
};

class AvifRgbImage : public avifRGBImage {
 public:
  AvifRgbImage(const avifImage* yuv, int rgbDepth, avifRGBFormat rgbFormat);
  ~AvifRgbImage() { avifRGBImageFreePixels(this); }
};

}  // namespace avif

namespace testutil {

inline bool Av1DecoderAvailable() { return true; }

std::vector<uint8_t> read_file(const char* file_name);

crabbyavif::ImagePtr CreateImage(int width, int height, int depth,
                                 avifPixelFormat yuv_format,
                                 avifPlanesFlags planes, avifRange yuv_range);

void FillImageGradient(avifImage* image, int offset);

double GetPsnr(const avifImage& image1, const avifImage& image2,
               bool ignore_alpha);

bool AreByteSequencesEqual(const uint8_t* data1, size_t data1_length,
                           const uint8_t* data2, size_t data2_length);

bool AreByteSequencesEqual(const avifRWData& data1, const avifRWData& data2);

bool ArePlanesEqual(const avifImage& image1, const avifImage& image2,
                    avifChannelIndex c);

bool AreImagesEqual(const avifImage& image1, const avifImage& image2,
                    bool ignore_alpha);

avifResult MergeGridFromRawPointers(int grid_cols, int grid_rows,
                                    const std::vector<const avifImage*>& cells,
                                    avifImage* merged);

avifResult MergeGrid(int grid_cols, int grid_rows,
                     const std::vector<crabbyavif::ImagePtr>& cells,
                     avifImage* merged);

void FillImageChannel(avifRGBImage* image, uint32_t channel_offset,
                      uint32_t value);

constexpr uint32_t kModifierSize = 4 * 4;

void ModifyImageChannel(avifRGBImage* image, uint32_t channel_offset,
                        const uint8_t modifier[kModifierSize]);

}  // namespace testutil
