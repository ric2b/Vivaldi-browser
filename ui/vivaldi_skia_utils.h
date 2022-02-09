// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#ifndef UI_VIVALDI_SKIA_UTILS_H_
#define UI_VIVALDI_SKIA_UTILS_H_

#include <string>
#include <vector>

#include "third_party/skia/include/core/SkBitmap.h"

namespace base {
class FilePath;
}

namespace vivaldi {
namespace skia_utils {

enum class ImageFormat {
  kPNG,
  kJPEG,
};

SkBitmap SmartCropAndSize(const SkBitmap& capture,
                          int target_width,
                          int target_height);

std::vector<unsigned char> EncodeBitmap(SkBitmap bitmap,
                                        ImageFormat image_format,
                                        int image_quality);

std::string EncodeBitmapAsDataUrl(SkBitmap bitmap,
                                  ImageFormat image_format,
                                  int image_quality);

base::FilePath EncodeBitmapToFile(base::FilePath directory,
                                  SkBitmap bitmap,
                                  ImageFormat image_format,
                                  int image_quality);

}  // namespace skia_utils
}  // namespace vivaldi

#endif  // UI_VIVALDI_SKIA_UTILS_H_
