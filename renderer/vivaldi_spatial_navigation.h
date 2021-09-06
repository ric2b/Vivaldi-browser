// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef RENDERER_SPATIAL_NAVIGATION_H_
#define RENDERER_SPATIAL_NAVIGATION_H_

#include <string>

#define INSIDE_BLINK 1

#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace gfx {
class Rect;
}

namespace vivaldi {

bool IsCovered(blink::Document* document, gfx::Rect& rect);
bool IsInViewport(blink::Document* document,
                  gfx::Rect& rect,
                  int window_height);
bool IsTooSmall(gfx::Rect& rect);
bool IsVisible(blink::WebElement element);
bool HasNavigableTag(blink::WebElement& element);
bool HasNavigableListeners(blink::WebElement& element);
bool IsNavigableElement(blink::WebElement& element);
gfx::Rect RevertDeviceScaling(gfx::Rect rect, float scale);
blink::IntRect FindImageElementRect(blink::WebElement element);
std::string ElementPath(blink::WebElement& element);

}

#endif  // RENDERER_SPATIAL_NAVIGATION_H_
