// Copyright (c) 2021 Vivaldi Technologies. All Rights Reserverd.

#include <dwmapi.h>
#include <Windows.h>

#include "base/strings/stringprintf.h"
#include "include/core/SkColor.h"
#include "ui/gfx/color_utils.h"
#include "skia/ext/skia_utils_win.h"

namespace {
std::string RgbToHexString(UINT8 red, UINT8 green, UINT8 blue) {
  // red, green and blue have values between 0 and 255
  return base::StringPrintf("#%02x%02x%02x", red, green, blue);
}
}  // namespace

namespace vivaldi {
std::string GetSystemAccentColor() {
  DWORD color = 0;
  BOOL opaque_blend = FALSE;
  HRESULT hr = DwmGetColorizationColor(&color, &opaque_blend);
  if (SUCCEEDED(hr)) {
    // 0xAARRGGBB format so we can use the SkColorGetX macros
    std::string color_string = RgbToHexString(
        SkColorGetR(color), SkColorGetG(color), SkColorGetB(color));
    return color_string;
  }
  return std::string();
}

std::string GetSystemHighlightColor() {
  COLORREF color_ref = ::GetSysColor(COLOR_HIGHLIGHT);
  SkColor color = skia::COLORREFToSkColor(color_ref);
  std::string color_string = RgbToHexString(
      SkColorGetR(color), SkColorGetG(color), SkColorGetB(color));
  return color_string;
}
}  // namespace vivaldi
