// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef RENDERER_VIVALDI_FRAME_SERVICE_IMPL_H_
#define RENDERER_VIVALDI_FRAME_SERVICE_IMPL_H_

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

#include "renderer/mojo/vivaldi_frame_service.mojom.h"

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
  void GetScrollPosition(GetScrollPositionCallback callback) override;
  void ScrollPage(vivaldi::mojom::ScrollType scroll_type,
                  int scroll_amount) override;
  void GetSpatialNavigationRects(
      GetSpatialNavigationRectsCallback callback) override;
  void GetFocusedElementInfo(GetFocusedElementInfoCallback callback) override;
  void DetermineTextLanguage(const std::string& text,
                             DetermineTextLanguageCallback callback) override;
  void ActivateElementFromPoint(int x, int y, int modifiers) override;
  void InsertText(const std::string& text) override;
  void ResumeParser() override;
  void RequestThumbnailForFrame(
      const ::gfx::Rect& rect,
      bool full_page,
      const ::gfx::Size& target_size,
      RequestThumbnailForFrameCallback callback) override;

 private:
  void BindService(
      mojo::PendingAssociatedReceiver<vivaldi::mojom::VivaldiFrameService>
          receiver);

  // Indirect owner.
  content::RenderFrame* const render_frame_;

  mojo::AssociatedReceiver<vivaldi::mojom::VivaldiFrameService> receiver_{this};
};

#endif  // RENDERER_VIVALDI_FRAME_SERVICE_IMPL_H_
