// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.

#include "renderer/blink/vivaldi_spatial_navigation_controller.h"

#include "content/public/renderer/render_frame.h"
#include "renderer/blink/vivaldi_spatial_navigation.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

const char* kSpatnavIndicatorStyle =
    "all: unset; "
    "position: absolute; "
    "box-shadow: 0 0 10px 4px rgba(255,122,0,0.70), 0 0 0 2px #FF7A00; "
    "z-index: 9999999; "
    "visibility: visible !important; ";

// Use a random number to avoid collision with an actual element id.
const char* kVivaldiIndicatorId = "vivaldi-indicator-212822325015";

class VivaldiSpatialNavigationController::ScrollListener
    : public blink::NativeEventListener {
 public:
  explicit ScrollListener(VivaldiSpatialNavigationController* controller,
                         blink::Document* document)
    : controller_(controller),
      document_(document) {}
  ~ScrollListener() override = default;

  void StartListening() {
    if (is_listening_) {
      return;
    }
    is_listening_ = true;

    blink::LocalDOMWindow* window = document_->domWindow();
    if (!window)
      return;

    window->addEventListener(blink::event_type_names::kScroll, this, true);
    if (blink::DOMWindow* outer_window = window->top()) {
      if (outer_window != window)
        outer_window->addEventListener(blink::event_type_names::kMousewheel,
                                       this, true);
    }
  }

  void StopListening() {
    if (!is_listening_) {
      return;
    }
    is_listening_ = false;

    if (!controller_) {
      return;
    }
    blink::Document* document = controller_->GetDocumentFromRenderFrame();
    if (!document) {
      return;
    }
    blink::LocalDOMWindow* window = document->domWindow();
    if (!window)
      return;

    if (!window->HasEventListeners(blink::event_type_names::kScroll)) {
      return;
    }

    window->removeEventListener(blink::event_type_names::kScroll, this, true);
    if (blink::DOMWindow* outer_window = window->top()) {
      if (outer_window != window) {
        outer_window->removeEventListener(blink::event_type_names::kScroll,
                                          this, true);
      }
    }
  }

 private:
  void Invoke(blink::ExecutionContext*, blink::Event* event) override {
    if (controller_) {
      controller_->UpdateIndicator(false, nullptr, event->target());
    }
  }
  VivaldiSpatialNavigationController* controller_;
  blink::Document* document_;
  bool is_listening_ = false;
};

VivaldiSpatialNavigationController::VivaldiSpatialNavigationController(
  content::RenderFrame* render_frame)
  : content::RenderFrameObserver(render_frame),
    render_frame_(render_frame) {
      scroll_listener_ = blink::MakeGarbageCollected<ScrollListener>(this,
        GetDocumentFromRenderFrame());
    }

VivaldiSpatialNavigationController::~VivaldiSpatialNavigationController() {}

void VivaldiSpatialNavigationController::FocusedElementChanged(
    const blink::WebElement& element) {
  if (!current_quad_) {
    HideIndicator();
    return;
  }

  blink::Element* current_element = current_quad_->GetElement();
  blink::Document* document = current_element ? current_element->ownerDocument()
                                              : GetDocumentFromRenderFrame();

  blink::Element* indicator = document->getElementById(
      WTF::AtomicString(kVivaldiIndicatorId));

  if (!indicator) {
    return;
  }

  blink::Element* old_container = GetScrollContainerForCurrentElement();

  if (element.IsNull()) {
    old_container->RemoveChild(indicator);
    return;
  }

  const blink::Element* new_element = element.ConstUnwrap<blink::Element>();

  if (new_element) {
    blink::Document* new_document = new_element->ownerDocument();

    if (document != new_document) {
      old_container->RemoveChild(indicator);
      HideIndicator();
      return;
    }
  }

  UpdateQuads();
  current_quad_ = GetQuadFromElement(new_element);

  blink::Element* new_container = GetScrollContainerForCurrentElement();

  if (old_container != new_container) {
    old_container->RemoveChild(indicator);
    new_container->AppendChild(indicator);
  }

  if (current_quad_) {
    UpdateIndicator(true, nullptr, nullptr);
  } else {
    HideIndicator();
  }
}

void VivaldiSpatialNavigationController::HideIndicator() {
  blink::Document* document = GetDocumentFromRenderFrame();
  blink::Element* indicator = document->getElementById(
      WTF::AtomicString(kVivaldiIndicatorId));
  if (!indicator) {
    return;
  }
  blink::Node* indicator_node = indicator;
  const blink::ComputedStyle* style =
      indicator_node->GetComputedStyleForElementOrLayoutObject();
  if (style) {
    blink::ComputedStyleBuilder builder(*style);
    builder.SetVisibility(blink::EVisibility::kHidden);
    indicator_node->GetLayoutObject()->SetStyle(builder.TakeStyle());
  }
}

void VivaldiSpatialNavigationController::CloseSpatnavOrCurrentOpenMenu(
    bool& layout_changed, bool& element_valid) {

  blink::Element* elm = current_quad_ ? current_quad_->GetElement() : nullptr;
  if (elm) {
    vivaldi::ClearHover(elm);
    layout_changed = UpdateQuads();

    // Re-check the element after un-hover.
    elm = current_quad_ ? current_quad_->GetElement() : nullptr;
    element_valid = elm ? true : false;
  }

  if (!layout_changed || !element_valid) {
    HideIndicator();
    scroll_listener_->StopListening();
  }
}

blink::Document*
VivaldiSpatialNavigationController::GetDocumentFromRenderFrame() {
  blink::WebLocalFrame* frame =
      render_frame_->GetWebView()->FocusedFrame();
  if (!frame) {
    return nullptr;
  }

  blink::Document* document =
      static_cast<blink::WebLocalFrameImpl*>(frame)->GetFrame()->GetDocument();
  return document;
}

vivaldi::QuadPtr VivaldiSpatialNavigationController::GetQuadFromElement(
    const blink::Element* element) {
  for (size_t i = 0; i < spatnav_quads_.size(); i++) {
    if (element == spatnav_quads_[i]->GetElement()) {
      return spatnav_quads_[i];
    }
  }
  return nullptr;
}

vivaldi::mojom::ScrollType
VivaldiSpatialNavigationController::ScrollTypeFromSpatnavDirection(
    vivaldi::mojom::SpatnavDirection direction) {
  using vivaldi::mojom::ScrollType;
  using vivaldi::mojom::SpatnavDirection;

  switch(direction) {
  case SpatnavDirection::kUp:
    return ScrollType::kUp;
  case SpatnavDirection::kLeft:
    return ScrollType::kLeft;
  case SpatnavDirection::kDown:
    return ScrollType::kDown;
  case SpatnavDirection::kRight:
    return ScrollType::kRight;
  // This case mimics previous implementation
  // Not sure the kNone case is reachable.
  case SpatnavDirection::kNone:
    return ScrollType::kRight;
  }
}

void VivaldiSpatialNavigationController::Scroll(
    ::vivaldi::mojom::ScrollType scroll_type,
    int scroll_amount) {
  using ScrollType = ::vivaldi::mojom::ScrollType;

  blink::Element* scroll_container =
	  GetDocumentFromRenderFrame()->ScrollingElementNoLayout();
  if (!scroll_container) {
    return;
  }

  blink::WebLocalFrame* web_local_frame =
      render_frame_->GetWebView()->FocusedFrame();

  if (!web_local_frame) {
    return;
  }

  switch (scroll_type) {
    case ScrollType::kUp:
      scroll_container->scrollBy(0.0, -scroll_amount);
      break;
    case ScrollType::kDown:
      scroll_container->scrollBy(0.0, scroll_amount);
      break;
    case ScrollType::kLeft:
      scroll_container->scrollBy(-scroll_amount, 0.0);
      break;
    case ScrollType::kRight:
      scroll_container->scrollBy(scroll_amount, 0.0);
      break;
    default:
      break;
  }
}

vivaldi::QuadPtr VivaldiSpatialNavigationController::NextQuadInDirection(
    vivaldi::mojom::SpatnavDirection direction) {
  using vivaldi::mojom::SpatnavDirection;

  switch (direction) {
    case SpatnavDirection::kUp:
      return current_quad_->NextUp();
    case SpatnavDirection::kDown:
      return current_quad_->NextDown();
    case SpatnavDirection::kLeft:
      return current_quad_->NextLeft();
    case SpatnavDirection::kRight:
      return current_quad_->NextRight();
    case vivaldi::mojom::SpatnavDirection::kNone:
      return nullptr;
  }
}

// Focuses an element if it's not an input field or similar. In that case we
// still do hover and sequential focus, i.e. this element will be tabbed from
// on next tab press.
//
// When scrolling with page up/down or arrow keys, blink will use the actual
// focused element for determining which scroll area gets the scroll.
// See: ScrollManager::LogicalScroll.
void VivaldiSpatialNavigationController::FocusElement(blink::Element* element) {
  // TODO: Currently radio buttons will steal arrows and shift+arrow when focused.
  //       Revisit this when we get spatnav mode underway and get more control
  //       over keyboard input events.
  if(!element->IsKeyboardFocusable() && !vivaldi::IsRadioButton(element)) {
    vivaldi::HoverElement(element);
    element->Focus();
  } else {
    blink::Document* document =
        element ? element->ownerDocument() : GetDocumentFromRenderFrame();
    vivaldi::HoverElement(element);
    document->SetSequentialFocusNavigationStartingPoint(element);
  }
}

void VivaldiSpatialNavigationController::ActivateElement(int modifiers) {
  blink::WebKeyboardEvent web_keyboard_event(
    blink::WebInputEvent::Type::kRawKeyDown, modifiers, base::TimeTicks());

  blink::KeyboardEvent* key_evt =
      blink::KeyboardEvent::Create(web_keyboard_event, nullptr);

  blink::Element* elm = current_quad_ ? current_quad_->GetElement() : nullptr;
  if (elm) {
    elm->Focus();
    elm->DispatchSimulatedClick(
        key_evt, blink::SimulatedClickCreationScope::kFromAccessibility);
  }
  HideIndicator();
}

bool VivaldiSpatialNavigationController::UpdateQuads() {
  blink::WebLocalFrame* frame = render_frame_->GetWebFrame();
  if (!frame) {
    return false;
  }

  float scale = render_frame_->GetWebView()->ZoomFactorForViewportLayout();
  if (scale == 0) {
    scale = 1.0;
  }

  blink::Element* current = nullptr;
  if (current_quad_ && current_quad_->GetElement()) {
    current = current_quad_->GetElement();
  }

  std::vector<blink::WebElement> spatnav_elements;
  blink::Document* document = frame->GetDocument();
  vivaldi::GetSpatialNavigationElements(document, scale, current, spatnav_elements);
  spatnav_quads_.clear();

  bool needs_update = false;
  if (spatnav_elements_.size() != spatnav_elements.size()) {
    needs_update = true;
  } else {
    for (unsigned i = 0; i < spatnav_elements.size(); i++) {
      if (spatnav_elements[i] != spatnav_elements_[i]) {
        needs_update = true;
      }
    }
  }

  if (needs_update) {
    spatnav_elements_.clear();

    for (unsigned i = 0; i < spatnav_elements.size(); i++)
      spatnav_elements_.push_back(spatnav_elements[i]);
  }

  if (spatnav_elements.size() == 0) {
    current_quad_ = nullptr;
    return true;
  }

  for (auto& element : spatnav_elements) {
    gfx::Rect rect = element.BoundsInWidget();
    if (element.IsLink()) {
      gfx::Rect r = vivaldi::FindImageElementRect(element);
      if (!r.IsEmpty()) {
        rect = r;
      }
    }
    rect = vivaldi::RevertDeviceScaling(rect, scale);
    std::string href = "";
    if (element.IsLink()) {
      blink::Element* blink_elm = element.Unwrap<blink::Element>();
      href = blink_elm->HrefURL().GetString().Utf8();
    }

    vivaldi::QuadPtr q = base::MakeRefCounted<vivaldi::Quad>(
        rect.x(), rect.y(), rect.width(), rect.height(), href, element);
    spatnav_quads_.push_back(q);
  }

  vivaldi::Quad::BuildPoints(spatnav_quads_);

  if (current_quad_) {
    // Checking if element is still there after update.
    current_quad_ = GetQuadFromElement(current_quad_->GetElement());
    if (current_quad_) {
      current_quad_->GetElement()->scrollIntoViewIfNeeded();
    }
  }

  return needs_update;
}

blink::Element*
VivaldiSpatialNavigationController::GetScrollContainerForCurrentElement() {
  if (!current_quad_) {
    return GetDocumentFromRenderFrame()->documentElement();
  }
  blink::Element* element = current_quad_->GetElement();
  if (!element) {
    return GetDocumentFromRenderFrame()->documentElement();
  }
  blink::LayoutBox* box = element->GetLayoutBoxForScrolling();
  while (element) {
    box = element->GetLayoutBoxForScrolling();
    if (box && box->IsUserScrollable()) {
      break;
    }
    if (!element->parentElement()) {
      break;
    }
    element = element->parentElement();
  }
  return element;
}

void VivaldiSpatialNavigationController::MoveRect(
    vivaldi::mojom::SpatnavDirection direction,
    blink::DOMRect* new_rect,
    std::string* href) {
  blink::Element* old_container = GetScrollContainerForCurrentElement();
  blink::Element* old_element =
      current_quad_ ? current_quad_->GetElement() : nullptr;

  bool needs_init = !current_quad_;
  // In case we have a previously focused element on the page, we unfocus it.
  // It can mess up element activation and looks confusing if it persists while
  // using spatnav.
  if (old_element) {
    blink::Document* old_document = old_element->ownerDocument();

    blink::Element* old_focus = old_document->FocusedElement();
    if (old_element != old_focus)
      needs_init = old_document != GetDocumentFromRenderFrame();
    if (old_document && old_focus)
      old_document->FocusedElement()->blur();
  }

  // Set this to true if we want quads to update at the end, which is AFTER
  // indicator has moved and current element updated.
  bool update_quads = false;

  if (needs_init) {
    UpdateQuads();
    current_quad_ = vivaldi::Quad::GetInitialQuad(spatnav_quads_, direction);

    if (!current_quad_) {
      blink::Document* document = GetDocumentFromRenderFrame();
      blink::LocalDOMWindow* window = document->domWindow();
      int scroll_amount = window->innerHeight() / 2;
      vivaldi::mojom::ScrollType scroll_type =
          ScrollTypeFromSpatnavDirection(direction);
      Scroll(scroll_type, scroll_amount);
      return;
    }
  } else {
    vivaldi::QuadPtr next_quad;
    next_quad = current_quad_ ? NextQuadInDirection(direction): nullptr;

    if (next_quad != nullptr) {

      current_quad_ = next_quad;
      blink::Element* elm = current_quad_->GetElement();
      if (elm) {
        elm->scrollIntoViewIfNeeded();
        update_quads = true;
      }
    } else {
      // If we found no quad in |direction| then scroll
      blink::Document* document = GetDocumentFromRenderFrame();
      blink::LocalDOMWindow* window = document->domWindow();
      int scroll_amount = window->innerHeight() / 2;
      vivaldi::mojom::ScrollType scroll_type =
          ScrollTypeFromSpatnavDirection(direction);
      Scroll(scroll_type, scroll_amount);
      update_quads = true;
    }
  }

  blink::Element* elm = nullptr;
  if (current_quad_) {
    elm = current_quad_->GetElement();
  }

  blink::Element* indicator = GetDocumentFromRenderFrame()->getElementById(
      WTF::AtomicString(kVivaldiIndicatorId));
  if (!indicator) {
    CreateIndicator();
  }
  blink::Element* new_container = GetScrollContainerForCurrentElement();

  // This can fail in some edge cases, like fullscreen videos.
  blink::Node* indicator_node = indicator;

  elm->GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      blink::DocumentUpdateReason::kSpatialNavigation);

  FocusElement(elm);

  if (old_container != new_container && indicator_node) {
    old_container->RemoveChild(indicator);
    new_container->AppendChild(indicator);
  }

  if (current_quad_) {
    *href = current_quad_->Href();
  }

  UpdateIndicator(true, new_rect, nullptr);
  if (update_quads) {
    UpdateQuads();
  }
}

void VivaldiSpatialNavigationController::CreateIndicator() {
  if (!current_quad_) {
    return;
  }

  blink::Element* elm = current_quad_->GetElement();
  blink::Document* document =
    elm ? elm->ownerDocument() : GetDocumentFromRenderFrame();

  blink::Element* indicator =
      document->CreateRawElement(blink::html_names::kDivTag);

  indicator->setAttribute(blink::html_names::kIdAttr,
                          WTF::AtomicString(kVivaldiIndicatorId));
  GetScrollContainerForCurrentElement()->AppendChild(indicator);

  // Set Style directly to avoid injection blocking.
  indicator->style()->setCSSText(document->GetExecutionContext(),
                                 kSpatnavIndicatorStyle, ASSERT_NO_EXCEPTION);

  indicator_ = indicator;

  scroll_listener_ =
      blink::MakeGarbageCollected<ScrollListener>(this, document);
  scroll_listener_->StartListening();
}

void VivaldiSpatialNavigationController::UpdateIndicator(bool resize,
    blink::DOMRect* new_rect, blink::EventTarget* target) {
  if (!current_quad_) {
    return;
  }
  blink::Element* elm = current_quad_->GetElement();
  blink::Document* document = elm->ownerDocument();
  blink::LocalDOMWindow* window = document->domWindow();
  if (!window) {
    return;
  }
  blink::Element* container = GetScrollContainerForCurrentElement();
  blink::LayoutBox* layout_box = container->GetLayoutBoxForScrolling();
  if (target && layout_box && target != container &&
      layout_box->IsUserScrollable()) {
    return;
  }

  float xoffset = 0;
  float yoffset = 0;
  if (container == document->documentElement()) {
    xoffset += window->scrollX();
    yoffset += window->scrollY();
  }

  blink::DOMRect* cr = container->GetBoundingClientRect();
  blink::Node* container_node = container;
  blink::Node* indicator_node = indicator_;

  const blink::ComputedStyle* indicator_style =
      indicator_node->IsElementNode()
          ? indicator_node->GetComputedStyleForElementOrLayoutObject()
          : nullptr;
  if (!indicator_style) {
    return;
  }

  // Update for zoom.
  float effective_zoom = indicator_style->EffectiveZoom();
  xoffset *= effective_zoom;
  yoffset *= effective_zoom;

  // Any parent of our container node will add its own offset if the position
  // is set to fixed.
  while (container_node->parentElement()) {
    blink::PhysicalRect cnr = container_node->BoundingBox();
    if (container_node->GetComputedStyleForElementOrLayoutObject()
            ->GetPosition() == blink::EPosition::kFixed) {
      xoffset -= cnr.X().ToDouble() * effective_zoom;
      yoffset -= cnr.Y().ToDouble() * effective_zoom;
    }
    container_node = container_node->parentElement();
  }

  if (layout_box) {
    if(container->GetComputedStyle()->Display() != blink::EDisplay::kBlock) {
      xoffset += -cr->x();
      yoffset += -cr->y();
      xoffset += layout_box->ScrolledContentOffset().left.ToDouble();
      yoffset += layout_box->ScrolledContentOffset().top.ToDouble();
    }
    if (layout_box->IsOutOfFlowPositioned()) {
      xoffset += layout_box->ScrolledContentOffset().left.ToDouble();
      yoffset += layout_box->ScrolledContentOffset().top.ToDouble();
    }
  }


  blink::ComputedStyleBuilder builder(
      *(indicator_node->GetComputedStyleForElementOrLayoutObject()));

  // When updating because of scrolling we already have the right size,
  // so we use this to skip some work.
  if (!resize) {
    blink::WebElement web_element = current_quad_->GetWebElement();
    gfx::Rect wr = web_element.BoundsInWidget();
    builder.SetWidth(blink::Length::Fixed(wr.width() - 4));
    builder.SetHeight(blink::Length::Fixed(wr.height() - 4));
    builder.SetLeft(blink::Length::Fixed(static_cast<int>(xoffset + wr.x() + 2)));
    builder.SetTop(blink::Length::Fixed(static_cast<int>(yoffset + wr.y() + 2)));

    if (new_rect) {
      new_rect->setX(wr.x());
      new_rect->setY(wr.y());
    }
  }

  else {
    blink::WebElement web_element = current_quad_->GetWebElement();
    gfx::Rect wr = web_element.BoundsInWidget();
    if (web_element.IsLink()) {
      gfx::Rect img_rect = vivaldi::FindImageElementRect(web_element);
      if (!img_rect.IsEmpty()) {
        wr = img_rect;
      }
    }
    builder.SetWidth(blink::Length::Fixed(wr.width() - 4));
    builder.SetHeight(blink::Length::Fixed(wr.height() - 4));
    builder.SetLeft(
        blink::Length::Fixed(static_cast<int>(xoffset + (int)wr.x() + 2)));
    builder.SetTop(blink::Length::Fixed(static_cast<int>(yoffset + wr.y() + 2)));
    if (new_rect) {
      new_rect->setX(wr.x());
      new_rect->setY(wr.y());
      new_rect->setWidth(wr.width());
      new_rect->setHeight(wr.height());
    }

    builder.SetVisibility(blink::EVisibility::kVisible);
  }

  indicator_node->GetLayoutObject()->SetStyle(builder.TakeStyle());
}
