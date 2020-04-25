// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#ifndef UI_VIVALDI_SKIA_UTILS_H_
#define UI_VIVALDI_SKIA_UTILS_H_

#include "chromium/third_party/skia/include/core/SkBitmap.h"

namespace vivaldi {
namespace skia_utils {

extern SkBitmap SmartCropAndSize(const SkBitmap& capture,
                                 int target_width,
                                 int target_height);

}  // namespace skia_utils
}  // namespace vivaldi

#endif  // UI_VIVALDI_SKIA_UTILS_H_
