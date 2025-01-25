// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/ui/window_sizer/window_sizer.h"

#include "build/build_config.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom-shared.h"
#include "ui/display/display.h"

namespace vivaldi {
const float kVivaldiAdditionalWidthFactor = 1.2f;  // 20% addition to width

struct VivaldiResolutionForMaximized {
  int width;
  int height;
  float scale_factor;
  bool ignore_scale;
};

const static struct VivaldiResolutionForMaximized vivaldi_maximized[] = {
    {1920, 1080, 1.0f, true},    // Typical monitor/laptop resolution
    {2560, 1440, 1.25f, false},  // Typical monitor resolution
    {3400, 1800, 2.5f, false},   // Typical Lenovo Yoga 3 Pro res
};

static const int kVivaldiResElmCount =
    sizeof(vivaldi_maximized) / sizeof(VivaldiResolutionForMaximized);

bool SetMaximizedIfPossible(gfx::Rect* bounds,
                            ui::mojom::WindowShowState* show_state,
                            const display::Display& display) {
  const gfx::Rect display_bounds = display.bounds();

  for (int i = 0; i < kVivaldiResElmCount; i++) {
    if (display.device_scale_factor() == vivaldi_maximized[i].scale_factor &&
        display_bounds.width() == vivaldi_maximized[i].width &&
        display_bounds.height() == vivaldi_maximized[i].height) {
      *show_state = ui::mojom::WindowShowState::kMaximized;
      return true;
    } else if (vivaldi_maximized[i].ignore_scale &&
               display_bounds.width() == vivaldi_maximized[i].width &&
               display_bounds.height() == vivaldi_maximized[i].height) {
      *show_state = ui::mojom::WindowShowState::kMaximized;
      return true;
    }
  }
#if !BUILDFLAG(IS_MAC)
  // If we're lower resolution, always maximize, except on Mac.
  if (display_bounds.width() < 1920 && display_bounds.height() < 1080) {
    *show_state = ui::mojom::WindowShowState::kMaximized;
    return true;
  }
#endif  // !IS_MAC
  return false;
}

}  // namespace vivaldi

void WindowSizer::AdjustDefaultSizeForVivaldi(
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state,
    const display::Display& display) const {
  vivaldi::SetMaximizedIfPossible(bounds, show_state, display);

  // Apply the increased size for when the user restores from maximized.
  if (display.bounds().width() >=
      bounds->width() * vivaldi::kVivaldiAdditionalWidthFactor) {
    bounds->set_width(bounds->width() * vivaldi::kVivaldiAdditionalWidthFactor);
  }
}
