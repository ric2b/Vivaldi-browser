// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "renderer/vivaldi_spatial_navigation.h"

#include <string>

#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace vivaldi {

bool IsCovered(blink::Document* document, blink::WebRect& rect) {
  int x = rect.x + rect.width / 2;
  int y = rect.y + rect.height / 2;

  blink::Element* cover_elm = document->ElementFromPoint((double)x, (double)y);

  if (!cover_elm) {
    return true;
  }
  return false;
}

bool IsInViewport(blink::Document* document,
                  blink::WebRect& rect,
                  int window_height) {
  int right = rect.x + rect.width;
  int bottom = rect.y + rect.height;
  int clientWidth = document->documentElement()->clientWidth();
  int clientHeight = document->documentElement()->clientHeight();

  int maxAbove = window_height;
  int maxBelow = maxAbove * 2;

  if (bottom > -maxAbove && rect.y <= maxBelow && rect.x >= 0 &&
    right <= clientWidth && right >= 0 && rect.width <= clientWidth &&
    rect.height <= clientHeight) {
    return true;
  }
  return false;
}

bool IsTooSmall(blink::WebRect& rect) {
  if (rect.width < 2 || rect.height < 2) {
    return true;
  }
  return false;
}

bool IsVisible(blink::WebElement element) {
  blink::WebNode web_node = element;
  const blink::Node *node = web_node.ConstUnwrap<blink::Node>();
  if (node) {
    const blink::LayoutObject *layout_object = node->GetLayoutObject();
    if (layout_object) {
      blink::PhysicalRect rect = blink::PhysicalRect::EnclosingRect(
        layout_object->LocalBoundingBoxRectForAccessibility());
      layout_object->MapToVisualRectInAncestorSpace(nullptr, rect);
      if (rect.IsEmpty()) {
        return false;
      }
    }
  }
  return true;
}

bool HasNavigableTag(blink::WebElement& element) {
  bool is_navigable_tag = false;

  if (element.GetAttribute("role") == "button") {
    return true;
  }
  if (element.HasHTMLTagName("a")) {
    if (!element.GetAttribute("href").IsEmpty() || element.IsLink()) {
      is_navigable_tag = true;
    }
  } else if (element.HasHTMLTagName("input") ||
             element.HasHTMLTagName("button") ||
             element.HasHTMLTagName("select") ||
             element.HasHTMLTagName("textarea")) {
    is_navigable_tag = true;
  }

  if (is_navigable_tag) {
    return true;
  }
  return false;
}

// Used For HiDPI displays. We need the unscaled version of the coordinates.
// See VB-63938.
blink::WebRect RevertDeviceScaling(blink::WebRect rect, float scale) {
  rect.x /= scale;
  rect.y /= scale;
  rect.width /= scale;
  rect.height /= scale;
  return rect;
}

// If a link contains an image, use the image rect.
blink::IntRect FindImageElementRect(blink::WebElement element) {
  blink::WebNode web_node = element;
  blink::Node *node = web_node.Unwrap<blink::Node>();
  node = blink::DynamicTo<blink::ContainerNode>(node);

  blink::Node *n = blink::FlatTreeTraversal::FirstChild(*node);
  while (n) {
    if (blink::IsA<blink::HTMLImageElement>(*n)) {
      return n->PixelSnappedBoundingBox();
      break;
    }
    n = blink::FlatTreeTraversal::Next(*n, node);
  }

  return blink::IntRect();
}

std::string ElementPath(blink::WebElement& element) {
  std::stringstream path;
  blink::WebNode node = element;
  bool first = true;

  while (!node.IsNull()) {
    if (!node.IsElementNode()) {
      node = node.ParentNode();
      continue;
    }
    if (!first) {
      path << "/";
    }

    blink::WebElement node_element = node.To<blink::WebElement>();
    if (node_element.HasAttribute("id")) {
      path << "#" << node_element.GetAttribute("id").Utf8();
    } else {
      int count = 1;
      blink::WebNode sibling = node.PreviousSibling();
      while (!sibling.IsNull()) {
        sibling = sibling.PreviousSibling();
        count++;
      }
      path << node_element.TagName().Utf8()<< "(" << count << ")";
    }
    node = node.ParentNode();
    first = false;
  }
  return path.str();
}

}
