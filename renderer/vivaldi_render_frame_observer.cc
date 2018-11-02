#include "renderer/vivaldi_render_frame_observer.h"

#include "third_party/WebKit/public/web/WebElement.h"
#include "renderer/vivaldi_render_messages.h"

namespace vivaldi {

VivaldiRenderFrameObserver::VivaldiRenderFrameObserver(content::RenderFrame* render_frame)
  : content::RenderFrameObserver(render_frame) {}

VivaldiRenderFrameObserver::~VivaldiRenderFrameObserver() {}

void VivaldiRenderFrameObserver::FocusedNodeChanged(const blink::WebNode& node)
{
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

void VivaldiRenderFrameObserver::OnDestruct()
{
  delete this;
}

}  // namespace vivaldi
