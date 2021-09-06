// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef EXTENSIONS_HELPER_VIVALDI_RENDER_FRAME_OBSERVER_H_
#define EXTENSIONS_HELPER_VIVALDI_RENDER_FRAME_OBSERVER_H_

#include "content/public/renderer/render_frame_observer.h"

#include "renderer/vivaldi_render_messages.h"

namespace vivaldi {

class VivaldiRenderFrameObserver
    : public content::RenderFrameObserver {
 public:
  explicit VivaldiRenderFrameObserver(content::RenderFrame* render_frame);
  ~VivaldiRenderFrameObserver() override;

  // content::RenderFrameObserver
  bool OnMessageReceived(const IPC::Message& message) override;

  void GetFocusedElementInfo(std::string* tagname,
                             std::string* type,
                             bool* editable,
                             std::string* role);

  private:
   void FocusedElementChanged(const blink::WebElement& element) override;
   void OnResumeParser();

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

   void OnDestruct() override;
};

}  // namespace vivaldi

#endif  //  EXTENSIONS_HELPER_VIVALDI_RENDER_FRAME_OBSERVER_H_
