#include "renderer/vivaldi_render_frame_observer.h"

#include "content/public/renderer/render_frame.h"
#include "renderer/vivaldi_render_messages.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace vivaldi {

VivaldiRenderFrameObserver::VivaldiRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

VivaldiRenderFrameObserver::~VivaldiRenderFrameObserver() {}

bool VivaldiRenderFrameObserver::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(VivaldiRenderFrameObserver, message)
    IPC_MESSAGE_HANDLER(VivaldiFrameHostMsg_ResumeParser, OnResumeParser)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void VivaldiRenderFrameObserver::OnResumeParser() {
  blink::WebDocumentLoader* loader =
      render_frame()->GetWebFrame()->GetProvisionalDocumentLoader()
          ? render_frame()->GetWebFrame()->GetProvisionalDocumentLoader()
          : render_frame()->GetWebFrame()->GetDocumentLoader();

  if (!loader) {
    NOTREACHED();
    return;
  }
  loader->ResumeParser();
}

void VivaldiRenderFrameObserver::FocusedNodeChanged(
    const blink::WebNode& node) {
  std::string tagname = "";
  std::string type = "";
  bool editable = false;
  std::string role = "";

  if (!node.IsNull() && node.IsElementNode()) {
    blink::WebElement element =
        const_cast<blink::WebNode&>(node).To<blink::WebElement>();
    tagname = element.TagName().Utf8();
    if (element.HasAttribute("type")) {
      type = element.GetAttribute("type").Utf8();
    }
    editable = element.IsEditable();

    if (element.HasAttribute("role")) {
      role = element.GetAttribute("role").Utf8();
    }
  }
  Send(new VivaldiMsg_DidUpdateFocusedElementInfo(routing_id(), tagname, type,
                                                  editable, role));
}

void VivaldiRenderFrameObserver::OnDestruct() {
  delete this;
}

}  // namespace vivaldi
