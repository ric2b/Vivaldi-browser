// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef RENDERER_SPATIAL_NAVIGATION_H_
#define RENDERER_SPATIAL_NAVIGATION_H_

#include <string>

#define INSIDE_BLINK 1

#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace vivaldi {

bool IsCovered(blink::Document* document, blink::WebRect& rect);
bool IsInViewport(blink::Document* document,
                  blink::WebRect& rect,
                  int window_height);
bool IsTooSmall(blink::WebRect& rect);
bool IsVisible(blink::WebElement element);
bool HasNavigableTag(blink::WebElement& element);
bool HasNavigableListeners(blink::WebElement& element);
bool IsNavigableElement(blink::WebElement& element);
blink::WebRect RevertDeviceScaling(blink::WebRect rect, float scale);
blink::IntRect FindImageElementRect(blink::WebElement element);
std::string ElementPath(blink::WebElement& element);

}

#endif  // RENDERER_SPATIAL_NAVIGATION_H_
