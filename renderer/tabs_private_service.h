// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef RENDERER_TABS_PRIVATE_SERVICE_H_
#define RENDERER_TABS_PRIVATE_SERVICE_H_

#include "base/macros.h"
#include "renderer/mojo/vivaldi_tabs_private.mojom.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace vivaldi {

class VivaldiTabsPrivateService
   : public vivaldi::mojom::VivaldiTabsPrivate {

 public:
  VivaldiTabsPrivateService(content::RenderFrame* render_frame);
  ~VivaldiTabsPrivateService() override;

  // Mojo implementation
  void GetAccessKeysForPage(GetAccessKeysForPageCallback callback) override;
  void GetScrollPosition(GetScrollPositionCallback callback) override;
  void GetSpatialNavigationRects(
      GetSpatialNavigationRectsCallback callback) override;
  void DetermineTextLanguage(const std::string& text,
                             DetermineTextLanguageCallback callback) override;

  void BindTabsPrivateService(
      mojo::PendingAssociatedReceiver<
          vivaldi::mojom::VivaldiTabsPrivate> receiver);

 private:
   content::RenderFrame* render_frame_;

  mojo::AssociatedReceiver<
      vivaldi::mojom::VivaldiTabsPrivate>
      receiver_{this};
};

}  // namespace vivaldi

#endif  // RENDERER_TABS_PRIVATE_SERVICE_H_
