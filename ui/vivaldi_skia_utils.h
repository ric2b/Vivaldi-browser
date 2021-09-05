// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#ifndef UI_VIVALDI_SKIA_UTILS_H_
#define UI_VIVALDI_SKIA_UTILS_H_

#include "third_party/skia/include/core/SkBitmap.h"

namespace vivaldi {
namespace skia_utils {

enum class ImageFormat {
    kPNG,
    kJPEG,
};

SkBitmap SmartCropAndSize(const SkBitmap& capture,
                                 int target_width,
                                 int target_height);

bool EncodeBitmap(const SkBitmap& bitmap,
                  ImageFormat image_format,
                  int image_quality,
                  std::vector<unsigned char>& data,
                  std::string& mime_type);

}  // namespace skia_utils
}  // namespace vivaldi

#endif  // UI_VIVALDI_SKIA_UTILS_H_
