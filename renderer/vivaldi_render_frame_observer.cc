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
      render_frame()->GetWebFrame()->GetDocumentLoader();

  if (!loader) {
    NOTREACHED();
    return;
  }
  loader->ResumeParser();
}

void VivaldiRenderFrameObserver::FocusedElementChanged(
    const blink::WebElement& element) {
  // We get an extra focus message when the Document gets changed, e.g. by
  // clicking a new iframe. In this case |element| is null and we should
  // ignore it so that we don't lose focus of the actual element. See VB-72174.
  if (element.IsNull()) {
    return;
  }

  std::string tagname = element.TagName().Utf8();
  std::string type =
      element.HasAttribute("type") ? element.GetAttribute("type").Utf8() : "";
  bool editable = element.IsEditable();
  std::string role =
      element.HasAttribute("role") ? element.GetAttribute("role").Utf8() : "";

  Send(new VivaldiMsg_DidUpdateFocusedElementInfo(routing_id(), tagname, type,
                                                  editable, role));
}

void VivaldiRenderFrameObserver::OnDestruct() {
  delete this;
}

}  // namespace vivaldi
