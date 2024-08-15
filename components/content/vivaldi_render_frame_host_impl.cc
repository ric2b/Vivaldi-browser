// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/text_input_manager.h"

#include "content/browser/renderer_host/frame_tree_node.h"

#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace content {

const mojo::AssociatedRemote<vivaldi::mojom::VivaldiFrameService>&
RenderFrameHostImpl::GetVivaldiFrameService() {
  if (!vivaldi_frame_service_)
    GetRemoteAssociatedInterfaces()->GetInterface(&vivaldi_frame_service_);
  return vivaldi_frame_service_;
}

void RenderFrameHostImpl::VisibleTextSelectionChanged(
    const std::u16string& text) {
  GetRenderWidgetHost()->VisibleTextSelectionChanged(text);
}

void RenderWidgetHostImpl::VisibleTextSelectionChanged(
    const std::u16string& text) {
  if (view_) {
    view_->VisibleTextSelectionChanged(text);
  }
}

void RenderWidgetHostViewBase::VisibleTextSelectionChanged(
    const std::u16string& text) {
  if (GetTextInputManager()) {
    GetTextInputManager()->VisibleSelectionChanged(this, text);
  }
}

const std::u16string* RenderWidgetHostViewBase::GetVisibleSelectedText() {
  if (!GetTextInputManager())
    return nullptr;
  return GetTextInputManager()->GetVisibleTextSelection(this);
}

const std::u16string* TextInputManager::GetVisibleTextSelection(
    RenderWidgetHostViewBase* view) const {
  DCHECK(!view || IsRegistered(view));
  return (view && IsRegistered(view)) ? &visible_text_selection_map_.at(view)
                                      : nullptr;
}

void TextInputManager::VisibleSelectionChanged(RenderWidgetHostViewBase* view,
                                               const std::u16string& text) {
  DCHECK(IsRegistered(view));
  visible_text_selection_map_[view] = std::u16string(text);
}

}  // namespace content
