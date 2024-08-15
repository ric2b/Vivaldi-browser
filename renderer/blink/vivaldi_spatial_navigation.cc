// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "renderer/blink/vivaldi_spatial_navigation.h"

#include <string>

#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_all_collection.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "ui/gfx/geometry/rect.h"

namespace vivaldi {

namespace {


// Borrowed from chromium spatnav code, but cannot be #included
// because it is private.
bool IsUnobscured(const blink::Element* element) {
  const blink::Node* node = element;
  if (!node) {
    return false;
  }
  const blink::LocalFrame* local_main_frame =
      blink::DynamicTo<blink::LocalFrame>(
      node->GetDocument().GetPage()->MainFrame());
  if (!local_main_frame)
    return false;

  if (node->IsMediaElement())
    return true;

  blink::PhysicalRect viewport_rect(
      local_main_frame->GetPage()->GetVisualViewport().VisibleContentRect());
  blink::PhysicalRect interesting_rect =
      Intersection(NodeRectInRootFrame(node), viewport_rect);

  if (interesting_rect.IsEmpty())
    return false;

  blink::HitTestLocation location(interesting_rect);

  blink::HitTestResult result =
      local_main_frame->GetEventHandler().HitTestResultAtLocation(
          location, blink::HitTestRequest::kReadOnly |
                        blink::HitTestRequest::kListBased |
                        blink::HitTestRequest::kIgnoreZeroOpacityObjects |
                        blink::HitTestRequest::kAllowChildFrameContent);

  const blink::HTMLFrameOwnerElement* frame_owner =
    blink::DynamicTo<blink::HTMLFrameOwnerElement>(node);

  const blink::HitTestResult::NodeSet& nodes = result.ListBasedTestResult();
  for (auto hit_node = nodes.rbegin(); hit_node != nodes.rend(); ++hit_node) {
    if (node->ContainsIncludingHostElements(**hit_node))
      return true;

    if (frame_owner &&
        frame_owner
            ->contentDocument()
            ->ContainsIncludingHostElements(**hit_node))
      return true;
  }
  return false;
}

bool HasFocusableChildren(blink::Element* elm) {
  for (blink::Node* node = blink::FlatTreeTraversal::FirstChild(*elm); node;
       node = blink::FlatTreeTraversal::Next(*node, elm)) {
    if (blink::Element* element = blink::DynamicTo<blink::Element>(node)) {
      if (element->IsFocusable()) {
        return true;
      }
    }
  }
  return false;
}

bool HasNavigableListeners(blink::WebElement& element) {
  blink::Element* elm = element.Unwrap<blink::Element>();

  if (!elm) {
    return false;
  }

  if (!elm->GetLayoutObject()) {
    return false;
  }

  if (elm->HasJSBasedEventListeners(blink::event_type_names::kClick) ||
      elm->HasJSBasedEventListeners(blink::event_type_names::kKeydown) ||
      elm->HasJSBasedEventListeners(blink::event_type_names::kKeypress) ||
      elm->HasJSBasedEventListeners(blink::event_type_names::kKeyup) ||
      elm->HasJSBasedEventListeners(blink::event_type_names::kMouseover) ||
      elm->HasJSBasedEventListeners(blink::event_type_names::kMouseenter))
    return true;

  if (elm->GetComputedStyle() && elm->ParentComputedStyle()) {
    if (elm->GetComputedStyle()->Cursor() == blink::ECursor::kPointer &&
        elm->ParentComputedStyle()->Cursor() != blink::ECursor::kPointer) {
      return true;
    }
  }
  if (!elm->IsSVGElement())
    return false;

  return (elm->HasEventListeners(blink::event_type_names::kFocus) ||
          elm->HasEventListeners(blink::event_type_names::kBlur) ||
          elm->HasEventListeners(blink::event_type_names::kFocusin) ||
          elm->HasEventListeners(blink::event_type_names::kFocusout));
}

bool HasNavigableTag(blink::WebElement& element) {
  if (element.GetAttribute("role") == "button") {
    return true;
  }
  if (element.HasHTMLTagName("a")) {
    if (!element.GetAttribute("href").IsEmpty() || element.IsLink()) {
      return true;;
    }
  } else if (element.HasHTMLTagName("input") ||
             element.HasHTMLTagName("button") ||
             element.HasHTMLTagName("select") ||
             element.HasHTMLTagName("textarea")) {
    return true;
  }
  return false;
}


bool IsTooSmallOrBig(blink::Document* document, gfx::Rect& rect) {
  int clientWidth = document->documentElement()->clientWidth();
  int clientHeight = document->documentElement()->clientHeight();

  if (rect.width() <= 4 || rect.height() <= 4) {
    return true;
  }

  if (rect.width() >= clientWidth || rect.height() >= clientHeight) {
    return true;
  }

  return false;
}

void DispatchMouseMoveAt(blink::Element* element, gfx::PointF event_position) {
  blink::WebNode web_node = element;
  const blink::Node* node = web_node.ConstUnwrap<blink::Node>();
  if (!node) {
    return;
  }
  const blink::Page* page = node->GetDocument().GetPage();
  if (!page) {
    return;
  }
  const blink::LocalFrame* local_main_frame =
      blink::DynamicTo<blink::LocalFrame>(page->MainFrame());
  if (!local_main_frame) {
    return;
  }

  gfx::PointF event_position_screen = event_position;
  int click_count = 0;
  blink::WebMouseEvent fake_mouse_move_event(
      blink::WebInputEvent::Type::kMouseMove, event_position,
      event_position_screen, blink::WebPointerProperties::Button::kNoButton,
      click_count, blink::WebInputEvent::kRelativeMotionEvent,
      base::TimeTicks::Now());
  Vector<blink::WebMouseEvent> coalesced_events, predicted_events;

  DCHECK(local_main_frame);
  local_main_frame->GetEventHandler().HandleMouseMoveEvent(
      blink::TransformWebMouseEvent(local_main_frame->View(),
      fake_mouse_move_event),
      coalesced_events, predicted_events);
}

}  // namespace

bool IsVisible(blink::WebElement element) {
  blink::WebNode web_node = element;
  const blink::Node* node = web_node.ConstUnwrap<blink::Node>();
  if (node) {
    const blink::LayoutObject* layout_object = node->GetLayoutObject();
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

// Special cases where the element has focusable children but should still
// be navigable.
bool IsDateTimeOrFile(blink::WebElement element) {
  return element.GetAttribute("type").Utf8() == "date" ||
         element.GetAttribute("type").Utf8() == "time" ||
         element.GetAttribute("type").Utf8() == "file";
}

bool IsRadioButton(blink::Element* element) {
  return element->getAttribute(blink::html_names::kTypeAttr).Utf8() == "radio";
}

void HoverElement(blink::Element* element) {
  gfx::PointF event_position(-1, -1);
  if (element) {
    event_position = blink::RectInViewport(*element).origin();
    event_position.Offset(1, 1);
  }
  DispatchMouseMoveAt(element, event_position);
}

void ClearHover(blink::Element* element) {
  gfx::PointF event_position(-1, -1);
  DispatchMouseMoveAt(element, event_position);
}

// Used For HiDPI displays. We need the unscaled version of the coordinates.
// See VB-63938.
gfx::Rect RevertDeviceScaling(const gfx::Rect& rect, float scale) {
  return gfx::Rect(rect.x() / scale, rect.y() / scale, rect.width() / scale,
                   rect.height() / scale);
}

// If a link contains an image, use the image rect.
gfx::Rect FindImageElementRect(blink::WebElement element) {
  blink::WebNode web_node = element;
  blink::Node* node = web_node.Unwrap<blink::Node>();
  node = blink::DynamicTo<blink::ContainerNode>(node);

  blink::Node* n = blink::FlatTreeTraversal::FirstChild(*node);
  while (n) {
    if (blink::IsA<blink::HTMLImageElement>(*n)) {
      return n->PixelSnappedBoundingBox();
    }
    n = blink::FlatTreeTraversal::Next(*n, node);
  }

  return gfx::Rect();
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
      path << node_element.TagName().Utf8() << "(" << count << ")";
    }
    node = node.ParentNode();
    first = false;
  }
  return path.str();
}

// We pass in the currently focused element which gets an automatic pass.
// Sometimes it fails the IsObscured test because the indicator element is on
// top of it. If there is no focused element, just pass null.
std::vector<blink::WebElement> GetSpatialNavigationElements(
    blink::Document* document,
    float scale,
    blink::Element* current,
    std::vector<blink::WebElement>& spatnav_elements) {

  blink::WebElementCollection all_elements = document->all();

  for (blink::WebElement element = all_elements.FirstItem(); !element.IsNull();
       element = all_elements.NextItem()) {
    gfx::Rect rect = RevertDeviceScaling(element.BoundsInWidget(), scale);
    blink::Element* elm = element.Unwrap<blink::Element>();
    if (elm && current && elm == current) {
      spatnav_elements.push_back(element);
      continue;
    }
    if (elm &&
        (elm->IsFocusable() || HasNavigableListeners(element) ||
         HasNavigableTag(element))) {
      if (elm->IsFrameOwnerElement()) {
        auto* owner = blink::To<blink::HTMLFrameOwnerElement>(elm);
        blink::LocalFrame* subframe =
            blink::DynamicTo<blink::LocalFrame>(owner->ContentFrame());
        if (subframe) {
          blink::Document* subdocument = subframe->GetDocument();
          GetSpatialNavigationElements(subdocument, scale, current,
                                       spatnav_elements);
        }
      } else if (!IsTooSmallOrBig(document, rect) && IsUnobscured(elm) &&
                 IsVisible(element)) {
        if (!HasFocusableChildren(elm) || IsDateTimeOrFile(element)) {
          spatnav_elements.push_back(element);
        }
      }
    }
  }

  // We need this sorted for easier checking whether element lists are equal.
  std::sort(spatnav_elements.begin(), spatnav_elements.end());

  return spatnav_elements;
}

}  // namespace vivaldi
