// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef RENDERER_VIVALDI_FRAME_SERVICE_IMPL_H_
#define RENDERER_VIVALDI_FRAME_SERVICE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "renderer/blink/vivaldi_spatial_navigation_controller.h"
#include "renderer/blink/vivaldi_spatnav_quad.h"
#include "third_party/blink/public/web/web_element.h"

#include "renderer/mojo/vivaldi_frame_service.mojom.h"

using QuadPtr = scoped_refptr<vivaldi::Quad>;

namespace blink {
class WebLocalFrame;
}

namespace content {
class RenderFrame;
}

class VivaldiFrameServiceImpl : public vivaldi::mojom::VivaldiFrameService,
                                public base::SupportsUserData::Data {
 public:
  VivaldiFrameServiceImpl(content::RenderFrame* render_frame);
  ~VivaldiFrameServiceImpl() override;

  static void Register(content::RenderFrame* render_frame);

  // Mojo implementation
  void GetAccessKeysForPage(GetAccessKeysForPageCallback callback) override;
  void AccessKeyAction(const std::string& access_key) override;
  void ScrollPage(vivaldi::mojom::ScrollType scroll_type,
                  int scroll_amount) override;
  void MoveSpatnavRect(vivaldi::mojom::SpatnavDirection dir,
                       MoveSpatnavRectCallback callback) override;
  void GetFocusedElementInfo(GetFocusedElementInfoCallback callback) override;
  void DetermineTextLanguage(const std::string& text,
                             DetermineTextLanguageCallback callback) override;

  void ActivateSpatnavElement(int modifiers) override;
  void CloseSpatnavOrCurrentOpenMenu(
      CloseSpatnavOrCurrentOpenMenuCallback callback) override;

  void InsertText(const std::string& text) override;
  void ResumeParser() override;
  void RequestThumbnailForFrame(
      const ::gfx::Rect& rect,
      bool full_page,
      const ::gfx::Size& target_size,
      RequestThumbnailForFrameCallback callback) override;
  void SetSupportsDraggableRegions(bool supports_draggable_regions) override;

 private:
  blink::Document* GetDocument();
  void HideSpatnavIndicator();
  bool UpdateSpatnavQuads();
  QuadPtr NextQuadInDirection(
      vivaldi::mojom::SpatnavDirection direction);
  vivaldi::mojom::ScrollType ScrollTypeFromSpatnavDir(
      vivaldi::mojom::SpatnavDirection);

  std::unique_ptr<VivaldiSpatialNavigationController> spatnav_controller_;

  void BindService(
      mojo::PendingAssociatedReceiver<vivaldi::mojom::VivaldiFrameService>
          receiver);

  // Indirect owner.
  content::RenderFrame* const render_frame_;

  mojo::AssociatedReceiver<vivaldi::mojom::VivaldiFrameService> receiver_{this};
};

#endif  // RENDERER_VIVALDI_FRAME_SERVICE_IMPL_H_
