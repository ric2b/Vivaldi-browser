// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef RENDERER_VIVALDI_RENDER_VIEW_OBSERVER_H_
#define RENDERER_VIVALDI_RENDER_VIEW_OBSERVER_H_

#include "content/public/renderer/render_view_observer.h"
#include "renderer/vivaldi_render_messages.h"

#define INSIDE_BLINK 1

namespace vivaldi {

// This class holds the Vivaldi specific parts of RenderView, and has the same
// lifetime.
class VivaldiRenderViewObserver : public content::RenderViewObserver {
 public:
  explicit VivaldiRenderViewObserver(content::RenderView* render_view);
  ~VivaldiRenderViewObserver() override;

 private:
  // RenderViewObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() override;
  void OnInsertText(const base::string16& text);
  void OnPinchZoom(float scale, int x, int y);
  void OnRequestThumbnailForFrame(
      VivaldiViewMsg_RequestThumbnailForFrame_Params params);
  void OnGetSpatialNavigationRects();
  void OnGetScrollPosition();
  void OnGetAccessKeysForPage();
  void OnAccessKeyAction(std::string access_key);
  void OnScrollPage(std::string scroll_type, int scroll_amount);
  void OnActivateElementFromPoint(int x, int y, int modifiers);

  DISALLOW_COPY_AND_ASSIGN(VivaldiRenderViewObserver);
};

}  // namespace vivaldi

#endif  //  RENDERER_VIVALDI_RENDER_VIEW_OBSERVER_H_
