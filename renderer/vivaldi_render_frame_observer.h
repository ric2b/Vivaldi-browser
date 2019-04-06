#ifndef EXTENSIONS_HELPER_VIVALDI_RENDER_FRAME_OBSERVER_H_
#define EXTENSIONS_HELPER_VIVALDI_RENDER_FRAME_OBSERVER_H_

#include "content/public/renderer/render_frame_observer.h"

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
   void FocusedNodeChanged(const blink::WebNode& node) override;
   void OnResumeParser();
   void OnDestruct() override;
};

}  // namespace vivaldi

#endif  //  EXTENSIONS_HELPER_VIVALDI_RENDER_FRAME_OBSERVER_H_
