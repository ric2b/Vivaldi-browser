// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef RENDERER_BLINK_VIVALDI_SPATIAL_NAVIGATION_H_
#define RENDERER_BLINK_VIVALDI_SPATIAL_NAVIGATION_H_

#include <vector>

#include "third_party/blink/renderer/core/core_export.h"

#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"

namespace blink {
class WebLocalFrame;
}

namespace vivaldi {

gfx::Rect CORE_EXPORT RevertDeviceScaling(const gfx::Rect& rect, float scale);
blink::IntRect CORE_EXPORT FindImageElementRect(blink::WebElement element);
std::string CORE_EXPORT ElementPath(blink::WebElement& element);

std::vector<blink::WebElement> CORE_EXPORT
GetSpatialNavigationElements(blink::WebLocalFrame* frame, float scale);

}  // namespace vivaldi

#endif  // RENDERER_BLINK_VIVALDI_SPATIAL_NAVIGATION_H_
