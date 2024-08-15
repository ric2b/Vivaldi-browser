// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef RENDERER_BLINK_VIVALDI_SPATIAL_NAVIGATION_H_
#define RENDERER_BLINK_VIVALDI_SPATIAL_NAVIGATION_H_

#include <vector>

#include "third_party/blink/renderer/core/core_export.h"

#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"

namespace blink {
class WebLocalFrame;
}

namespace vivaldi {

gfx::Rect CORE_EXPORT RevertDeviceScaling(const gfx::Rect& rect, float scale);
gfx::Rect CORE_EXPORT FindImageElementRect(blink::WebElement element);
std::string CORE_EXPORT ElementPath(blink::WebElement& element);

bool CORE_EXPORT IsVisible(blink::WebElement element);
bool CORE_EXPORT IsRadioButton(blink::Element* element);

blink::ScrollableArea CORE_EXPORT *ScrollableAreaFor(const blink::Node* node);

void CORE_EXPORT HoverElement(blink::Element* element);
void CORE_EXPORT ClearHover(blink::Element* element);

std::vector<blink::WebElement> CORE_EXPORT
GetSpatialNavigationElements(blink::Document* document,
                             float scale,
                             blink::Element* current,
                             std::vector<blink::WebElement>& spatnav_elements);

}  // namespace vivaldi

#endif  // RENDERER_BLINK_VIVALDI_SPATIAL_NAVIGATION_H_
