// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.

#ifndef RENDERER_BLINK_VIVALDI_SPATIAL_NAVIGATION_CONTROLLER_H_
#define RENDERER_BLINK_VIVALDI_SPATIAL_NAVIGATION_CONTROLLER_H_

#include "renderer/blink/vivaldi_spatnav_quad.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"

#include "renderer/mojo/vivaldi_frame_service.mojom.h"

namespace content {
class RenderFrame;
}

class VivaldiSpatialNavigationController {
 public:
  VivaldiSpatialNavigationController(content::RenderFrame* render_frame);
  ~VivaldiSpatialNavigationController();

  blink::Document* GetDocumentFromRenderFrame();

  vivaldi::mojom::ScrollType ScrollTypeFromSpatnavDirection(
      vivaldi::mojom::SpatnavDirection direction);

  void Scroll(vivaldi::mojom::ScrollType scroll_type, int scroll_amount);

  vivaldi::QuadPtr NextQuadInDirection(
      vivaldi::mojom::SpatnavDirection direction);

  void HideIndicator();
  void ActivateElement(int modifiers);
  bool UpdateQuads();
  blink::Element* GetScrollContainerForCurrentElement();
  void MoveRect(vivaldi::mojom::SpatnavDirection direction,
                blink::DOMRect* new_rect);
  void CreateIndicator();
  void UpdateIndicator(bool resize, blink::DOMRect* new_rect,
                       blink::EventTarget* target);
  void CloseSpatnavOrCurrentOpenMenu(bool& layout_changed,
                                     bool& element_valid);

 private:
  class ScrollListener;

  std::vector<blink::WebElement> spatnav_elements_;
  std::vector<vivaldi::QuadPtr> spatnav_quads_;
  vivaldi::QuadPtr current_quad_;

  content::RenderFrame* const render_frame_;

  blink::Element* indicator_ = nullptr;

  VivaldiSpatialNavigationController::ScrollListener*
      scroll_listener_;
};

#endif  // RENDERER_BLINK_VIVALDI_SPATIAL_NAVIGATION_CONTROLLER_H_
